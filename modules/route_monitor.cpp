/*
 * Copyright (c) 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <znc/IRCNetwork.h>
#include <znc/IRCSock.h>
#include <znc/Modules.h>

namespace {

// SplitAndSend constructs commands and replies <= 512 octets, when the last
// parameter is a comma-separated list.
//
// Precondition: The length of base plus the length of any individual member of
// the list must be <= 512.
template <typename Iterator, typename ConvertToCStringFn>
void SplitAndSend(const CString &base, Iterator i_start, const Iterator &i_end,
                  ConvertToCStringFn asString, int targMax,
                  const std::function<void(const CString &)> &send) {
    while (i_start != i_end) {
        CString command = base + asString(*i_start);
        ++i_start;
        int num_targets = 1;
        while ((targMax == 0 || num_targets < targMax) && i_start != i_end &&
               (command.length() + 1 + asString(*i_start).length()) <= 512) {
            command += "," + asString(*i_start);
            ++i_start;
            ++num_targets;
        }
        send(command);
    }
}

// CasefoldedNick uses lowercased nick for comparisons.
// NOTE: Only supports ASCII casefolding since that is all CString::AsLower
// supports.
class CasefoldedNick {
  public:
    explicit CasefoldedNick(const CString &nick) {
        m_casefoldedNick = nick.AsLower();
    }

    const CString &GetCasefoldedNick() const { return m_casefoldedNick; }

    bool operator<(const CasefoldedNick &rhs) const {
        return m_casefoldedNick < rhs.m_casefoldedNick;
    }

    bool operator==(const CasefoldedNick &rhs) const {
        return m_casefoldedNick == rhs.m_casefoldedNick;
    }

  private:
    CString m_casefoldedNick;
};

// MonitorTarget stores state about a nick.
struct MonitorTarget {
    // Non-casefolded nick, excluding !user@host, if any.
    CString actualNick;

    // user@host if the server provided it. Excludes !.
    CString userHost;

    // Whether the target is known to be online or not.
    // Updated whenever we receive a 730/731 reply.
    bool online;
};

// AsActualNick converts casefolded nicks into actual nicks, using the
// targetState map.
class AsActualNick {
  public:
    // targetState map must outlive this class.
    explicit AsActualNick(
        const std::map<CasefoldedNick, MonitorTarget> *targetState)
        : m_targetState(targetState) {}

    const CString operator()(const CasefoldedNick &nick) const {
        auto it = m_targetState->find(nick);
        if (it == m_targetState->end()) {
            // This shouldn't happen, but we just fallback to the casefolded
            // nick in this case.
            return nick.GetCasefoldedNick();
        }
        return it->second.actualNick;
    }

  private:
    const std::map<CasefoldedNick, MonitorTarget> *m_targetState; // not owned
};

// Identity returns the string unchanged, when the nick is already not
// casefolded.
const CString &Identity(const CString &nick) { return nick; }

const int kMonitorSupportUnknown = -2;
const int kMonitorNotSupported = -1;

} // namespace

class CRouteMonitorMod : public CModule {
  public:
    MODCONSTRUCTOR(CRouteMonitorMod) {}

    bool OnLoad(const CString &, CString &) override {
        ClearState();

        if (!GetNetwork()->GetIRCSock() ||
            !GetNetwork()->GetIRCSock()->IsConnected()) {
            // Nothing to sync, since we're not connected.
            return true;
        }

        m_monitorLimit =
            GetNetwork()->GetIRCSock()->GetISupport("MONITOR", "-1").ToInt();

        if (m_monitorLimit >= 0) {
            m_syncing = true;
            PutIRC("MONITOR L");
        }

        return true;
    }

    void OnIRCConnected() override {
        // When OnIRCConnected() is called, ISUPPORT is not available yet.
        m_monitorLimit = kMonitorSupportUnknown;
        m_syncing = false;

        // Fill in m_replayNeeded with the set of existing subscriptions. Once
        // we see that the new server also supports MONITOR, we'll resend these
        // subscriptions.
        m_replayNeeded.clear();
        for (const auto& target : m_targetState) {
            m_replayNeeded.push_back(target.second.actualNick);
        }
    }

    void OnIRCDisconnected() override {
        m_monitorLimit = kMonitorSupportUnknown;
        m_syncing = false;
        m_replayNeeded.clear();
    }

    void OnClientDisconnect() override { ClearClientMonitorSubscriptions(); }

    EModRet OnNumericMessage(CNumericMessage &message) override {
        if (!GetNetwork()->GetIRCSock() ||
            !GetNetwork()->GetIRCSock()->IsConnected()) {
            return CONTINUE;
        }

        unsigned int numeric = message.GetCode();

        // Refresh ISUPPORT information if this is a monitor numeric or we're
        // waiting to replay subscriptions.
        // Don't refresh on numeric 5 (ISUPPORT) since this hasn't been
        // processed by ZNC core yet, and we get the information from ZNC core.
        if ((numeric >= 730 && numeric <= 733) ||
            (numeric != 5 && !m_replayNeeded.empty())) {
            UpdateMonitorSupport();
        }

        if (m_monitorLimit < 0) {
            // Network doesn't support MONITOR or we don't know yet if it does.
            return CONTINUE;
        }

        // Re-add subscriptions if any are waiting.
        if (!m_replayNeeded.empty()) {
            SplitAndSend("MONITOR + ", m_replayNeeded.begin(),
                         m_replayNeeded.end(), Identity, GetMonitorTargMax(),
                         [&](const CString &command) { PutIRC(command); });
            m_replayNeeded.clear();
        }

        if (numeric == 730 || numeric == 731) {
            const bool online = numeric == 730;

            CString nick = message.GetParam(0); // can be * per spec
            VCString targets = ParseTargets(message.GetParam(1));

            RouteMonitorUpdates(nick, targets, online);
            return HALTCORE;
        } else if (numeric == 732) {
            if (!m_syncing) {
                // Unexpected MONITOR L reply from the server. Pass it through.
                return CONTINUE;
            }

            VCString targets = ParseTargets(message.GetParam(1));

            SubscribeAllClientsToTargets(targets);
            return HALTCORE;
        } else if (numeric == 733) {
            if (!m_syncing) {
                // Unexpected MONITOR L reply from the server. Pass it through.
                return CONTINUE;
            }

            // Send MONITOR S to refresh state, if needed.
            m_syncing = false;
            if (!m_targetState.empty()) {
                PutIRC("MONITOR S");
            }

            return HALTCORE;
        }

        return CONTINUE;
    }

    EModRet OnUserRawMessage(CMessage &message) override {
        if (!m_pClient || !GetNetwork()->GetIRCSock() ||
            !GetNetwork()->GetIRCSock()->IsConnected()) {
            return CONTINUE;
        }

        // Only interested in MONITOR commands.
        if (!message.GetCommand().Equals("MONITOR")) {
            return CONTINUE;
        }

        // We shouldn't be getting MONITOR commands if monitor isn't supported.
        UpdateMonitorSupport();
        if (m_monitorLimit < 0) {
            PutModule("Network is missing ISUPPORT token for MONITOR. "
                      "route_monitor is inactive.");
            return CONTINUE;
        }

        const CString command = message.GetParam(0);

        if (command.Equals("+") || command.Equals("-")) {
            VCString targets;
            message.GetParam(1).Split(",", targets, false);
            if (command.Equals("+")) {
                AddMonitorSubscriptions(targets);
                return HALTCORE;
            } else {
                RemoveMonitorSubscriptions(targets.begin(), targets.end(),
                                           Identity);
                return HALTCORE;
            }
        } else if (command.Equals("C")) {
            ClearClientMonitorSubscriptions();
            return HALTCORE;
        } else if (command.Equals("L")) {
            ListClientMonitorSubscriptions();
            return HALTCORE;
        } else if (command.Equals("S")) {
            // Pass through directly to the server. This will result in the
            // statuses being broadcast to all subscribed clients, but since
            // they subscribed they should be able to handle it.
            return CONTINUE;
        }

        // Unrecognized command. Just pass it through to the server.
        PutModule("Unrecognized MONITOR command: " + message.ToString());
        return CONTINUE;
    }

  private:
    void RouteMonitorUpdates(const CString &userNick, const VCString &targets,
                             const bool online) {
        std::map<CClient *, std::set<CString /* actual nick */>> updatesToSend;

        // Update m_targetState and collect updatesToSend to clients.
        for (const CString &target : targets) {
            // Separate the !user@host if it exists.
            const auto nick = target.Token(0, false, "!");
            const auto casefoldedNick = CasefoldedNick(nick);

            auto it = m_targetClients.find(casefoldedNick);
            if (it == m_targetClients.end()) {
                // No subscriptions to this target? Maybe it was just removed.
                // Ignore it.
                continue;
            }

            MonitorTarget &targetState = m_targetState[casefoldedNick];
            // Store non-casefolded nick, including !user@host if the server
            // provided it.
            targetState.actualNick = nick;  // without !user@host
            targetState.userHost = target.Token(1, true, "!");
            targetState.online = online;

            for (CClient *subscribedClient : it->second) {
                updatesToSend[subscribedClient].insert(target);
            }
        }

        const CString base_reply = CString(":irc.znc.in ") +
                                   (online ? "730" : "731") + " " + userNick +
                                   " :";

        // Send updates to our ZNC clients.
        for (const auto &it : updatesToSend) {
            SplitAndSend(
                base_reply, it.second.begin(), it.second.end(), Identity, 0,
                [&](const CString &reply) { it.first->PutClient(reply); });
        }
    }

    void SubscribeAllClientsToTargets(const VCString& targets) {
        // Add all clients to these targets.
        const std::vector<CClient *> &clients = GetNetwork()->GetClients();
        for (const CString &target : targets) {
            // !user@host shouldn't be included, but remove it anyways.
            const auto nick = target.Token(0, false, "!");
            CasefoldedNick nickCasefolded(nick);

            // Assume offline. We will issue MONITOR S afterwards to update
            // this state.
            m_targetState[nickCasefolded] = {nick, "", false};
            for (CClient *client : clients) {
                m_clientTargets[client].insert(nickCasefolded);
                m_targetClients[nickCasefolded].insert(client);
            }
        }
    }

    void AddMonitorSubscriptions(const VCString &targets) {
        // Check monitor limit first.
        int numTargets = m_targetClients.size();
        for (const auto &target : targets) {
            if (!m_targetClients.count(CasefoldedNick(target))) {
                ++numTargets;
            }
        }
        if (m_monitorLimit > 0 && numTargets > m_monitorLimit) {
            m_pClient->PutClient(
                ":irc.znc.in 734 " + GetNetwork()->GetIRCSock()->GetNick() +
                " " + CString(m_monitorLimit) + " " +
                CString(",").Join(targets.begin(), targets.end()) +
                " :Monitor list is full.");
            return;
        }

        std::vector<CString> targetsToAdd;
        // Targets for which the state is already known because another ZNC
        // client already subscribed.
        std::vector<CString> cachedOnlineTargets, cachedOfflineTargets;
        for (const auto &target : targets) {
            if (AddMonitorSubscription(target)) {
                targetsToAdd.push_back(target);
            } else {
                // We're not the first client, so we need to return the cached
                // state for this target to the client, if it exists.
                std::map<CasefoldedNick, MonitorTarget>::const_iterator it =
                    m_targetState.find(CasefoldedNick(target));
                if (it != m_targetState.end()) {
                    CString targetToSend = it->second.actualNick;
                    if (it->second.userHost != "") {
                        targetToSend += "!" + it->second.userHost;
                    }

                    if (it->second.online) {
                        cachedOnlineTargets.push_back(targetToSend);
                    } else {
                        cachedOfflineTargets.push_back(targetToSend);
                    }
                }
            }
        }

        // Add new monitor subscriptions, if any.
        SplitAndSend("MONITOR + ", targetsToAdd.begin(), targetsToAdd.end(),
                     Identity, GetMonitorTargMax(),
                     [&](const CString &command) { PutIRC(command); });

        // Inform client about online and offline cached targets, if any.
        const CString nick = GetNetwork()->GetIRCSock()->GetNick();
        SplitAndSend(
            ":irc.znc.in 730 " + nick + " :", cachedOnlineTargets.begin(),
            cachedOnlineTargets.end(), Identity, 0,
            [&](const CString &reply) { m_pClient->PutClient(reply); });
        SplitAndSend(
            ":irc.znc.in 731 " + nick + " :", cachedOfflineTargets.begin(),
            cachedOfflineTargets.end(), Identity, 0,
            [&](const CString &reply) { m_pClient->PutClient(reply); });
    }

    // Adds an individual MONITOR subscription for a client.
    //
    // If we're already subscribed to this target, the caller should inform the
    // client of the current (cached) state.
    //
    // Returns whether this needs to be synced to the IRC server.
    bool AddMonitorSubscription(const CString &target) {
        CasefoldedNick targetCasefolded = CasefoldedNick(target);

        std::set<CasefoldedNick> &clientTargets = m_clientTargets[m_pClient];

        // If we're already subscribed to this target, nothing to do.
        if (clientTargets.count(targetCasefolded)) {
            return false;
        }

        clientTargets.insert(targetCasefolded);

        std::set<CClient *> &targetClients = m_targetClients[targetCasefolded];
        targetClients.insert(m_pClient);

        if (targetClients.size() == 1) {
            // Assume the client is offline for now. The server will tell us if
            // they are online.
            m_targetState[targetCasefolded] = {target, "", false};

            // As we're the first client subscribing to this target, we need to
            // tell the server about it. The server should reply with the
            // current state, which we will route to the client due to the map
            // updates above.
            return true;
        } else {
            // Server is already aware of this subscription.
            return false;
        }
    }

    template <typename Iterator, typename ConvertToCStringFn>
    void RemoveMonitorSubscriptions(Iterator i_start, const Iterator &i_end,
                                    ConvertToCStringFn asString) {
        std::vector<CString> serverTargets;
        for (; i_start != i_end; ++i_start) {
            CString targetConverted = asString(*i_start);
            if (RemoveMonitorSubscription(targetConverted)) {
                serverTargets.push_back(targetConverted);
            }
        }

        // Remove old monitor subscriptions, if any.
        SplitAndSend("MONITOR - ", serverTargets.begin(), serverTargets.end(),
                     Identity, // already converted above
                     GetMonitorTargMax(),
                     [&](const CString &command) { PutIRC(command); });
    }

    // Removes an individual MONITOR subscription for a client.
    // Returns whether this needs to be synced to the IRC server.
    bool RemoveMonitorSubscription(const CString &target) {
        CasefoldedNick targetCasefolded = CasefoldedNick(target);
        std::set<CasefoldedNick> &clientTargets = m_clientTargets[m_pClient];

        // If we're not subscribed to this target, nothing to do.
        if (!clientTargets.count(targetCasefolded)) {
            return false;
        }

        clientTargets.erase(targetCasefolded);

        std::set<CClient *> *targetClients = &m_targetClients[targetCasefolded];
        targetClients->erase(m_pClient);

        if (targetClients->size() == 0) {
            m_targetClients.erase(targetCasefolded);
            targetClients = nullptr;
            m_targetState.erase(targetCasefolded);
            // We were the last client subscribed to this target. Inform the
            // server about its removal.
            return true;
        } else {
            // We should remain subscribed to this target, since there is at
            // least one other client subscribed.
            return false;
        }
    }

    void ClearClientMonitorSubscriptions() {
        auto it = m_clientTargets.find(m_pClient);

        if (it == m_clientTargets.end()) {
            // Nothing to unsubscribe from.
            return;
        }

        // Make a copy of the clientTargets set, since the set in
        // m_clientTargets will be modified during iteration.
        std::set<CasefoldedNick> clientTargets = it->second;
        RemoveMonitorSubscriptions(clientTargets.begin(), clientTargets.end(),
                                   AsActualNick(&m_targetState));
    }

    void ListClientMonitorSubscriptions() {
        std::map<CClient *, std::set<CasefoldedNick>>::iterator it =
            m_clientTargets.find(m_pClient);

        if (it != m_clientTargets.end()) {
            const CString base_reply = CString(":irc.znc.in 732 ") +
                                       GetNetwork()->GetIRCSock()->GetNick() +
                                       " :";
            SplitAndSend(
                base_reply, it->second.begin(), it->second.end(),
                AsActualNick(&m_targetState), 0,
                [&](const CString &reply) { m_pClient->PutClient(reply); });
        }

        m_pClient->PutClient(":irc.znc.in 733 " +
                             GetNetwork()->GetIRCSock()->GetNick() +
                             " :End of MONITOR list");
    }

    VCString ParseTargets(CString targets) {
        VCString result;
        targets.TrimPrefix();
        targets.Split(",", result, false);
        return result;
    }

    void ClearState() {
        m_monitorLimit = kMonitorSupportUnknown;
        m_clientTargets.clear();
        m_targetClients.clear();
        m_targetState.clear();
        m_replayNeeded.clear();
        m_syncing = false;
    }
    
    // Precondition: We're connected to the server.
    void UpdateMonitorSupport() {
        m_monitorLimit =
            GetNetwork()->GetIRCSock()->GetISupport("MONITOR", "-1").ToInt();
    }

    // Returns the maximum number of targets allowed in a single command.
    // Precondition: We're connected to the server.
    int GetMonitorTargMax() {
        const CString targMax =
            GetNetwork()->GetIRCSock()->GetISupport("TARGMAX", "");
        if (targMax.empty()) {
            return 0; // unlimited
        }

        VCString targetTypes;
        targMax.Split(",", targetTypes, false);
        for (const CString &targetType : targetTypes) {
            if (!targetType.StartsWith("MONITOR:")) {
                continue;
            }
            int result = targetType.TrimPrefix_n("MONITOR:").ToInt();
            if (result < 0) {
                return 0;
            }
            return result;
        }

        return 0;
    }

    // The server-side limit on the total number of active targets allowed.
    // -2 means we do not yet know if the server supports MONITOR.
    // -1 means the server does not support MONITOR.
    // 0 means unlimited.
    int m_monitorLimit;

    // The set of targets monitored by each individual client.
    std::map<CClient *, std::set<CasefoldedNick>> m_clientTargets;

    // The set of clients monitoring a particular target.
    // Once a set becomes empty, we will unsubscribe from that target.
    std::map<CasefoldedNick, std::set<CClient *>> m_targetClients;

    // Cached state of monitor targets. This is populated when the state is
    // received from the server.
    std::map<CasefoldedNick, MonitorTarget> m_targetState;

    // MONITOR subscriptions that need to be resent to the server due to
    // reconnection. This is deferred until we see the ISUPPORT token for
    // MONITOR.
    VCString m_replayNeeded;

    // Whether we're syncing the client list.
    bool m_syncing;
};

template <> void TModInfo<CRouteMonitorMod>(CModInfo &Info) {
    Info.SetWikiPage("route_monitor");
}

NETWORKMODULEDEFS(CRouteMonitorMod,
                  "Send MONITOR status changes only to subscribed clients");
