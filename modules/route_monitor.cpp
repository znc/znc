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
                  ConvertToCStringFn asString,
                  const std::function<void(const CString &)> &send) {
    while (i_start != i_end) {
        CString command = base + asString(*i_start);
        ++i_start;
        while (i_start != i_end &&
               (command.length() + 1 + asString(*i_start).length()) <= 512) {
            command += "," + asString(*i_start);
            ++i_start;
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
    CString actualNick; // non-casefolded nick
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
        // TODO: Sync state with server. If the server supports MONITOR and
        // there are existing subscriptions, subscribe all clients to them.
        return true;
    }

    void OnIRCConnected() override {
        // When OnIRCConnected() is called, ISUPPORT may not be available yet.
        m_monitorLimit = kMonitorSupportUnknown;

        // TODO: Replay MONITOR + to server instead of clearing state.
        // This would need to be deferred until ISUPPORT is available.
        ClearState();
    }

    void OnIRCDisconnected() override {
        // TODO: Preserve state so we can replay MONITOR + on reconnect.
        ClearState();
    }

    void OnClientDisconnect() override { ClearClientMonitorSubscriptions(); }

    EModRet OnRawMessage(CMessage &message) override {
        if (!GetNetwork()->GetIRCSock() ||
            !GetNetwork()->GetIRCSock()->IsConnected()) {
            return CONTINUE;
        }

        if (m_monitorLimit < 0) {
            // Network doesn't support MONITOR or we don't know yet if it does.
            return CONTINUE;
        }

        CString rpl = message.GetCommand();

        if (rpl.Equals("730") || rpl.Equals("731")) {
            const bool online = rpl.Equals("730");

            CString nick = message.GetParam(0); // can be * per spec

            VCString targets;

            CString rplTargets = message.GetParam(1);
            rplTargets.TrimPrefix();
            rplTargets.Split(",", targets, false);

            RouteMonitorUpdates(nick, targets, online);
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

        m_monitorLimit =
            GetNetwork()->GetIRCSock()->GetISupport("MONITOR", "-1").ToInt();

        // TODO: Pull TARGMAX for MONITOR? We enforce the overall limit and
        // command length <= 512 octets but not the number of targets per
        // command.

        // We shouldn't be getting MONITOR commands if monitor isn't supported.
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
    void RouteMonitorUpdates(const CString &nick, const VCString &targets,
                             const bool online) {
        std::map<CClient *, std::set<CString /* actual nick */>> updatesToSend;

        // Update m_targetState and collect updatesToSend to clients.
        for (const CString &target : targets) {
            const auto casefoldedNick = CasefoldedNick(target);

            auto it = m_targetClients.find(casefoldedNick);
            if (it == m_targetClients.end()) {
                // No subscriptions to this target? Maybe it was just removed.
                // Ignore it.
                // TODO: For sync: this should trigger the subscription being
                // added for all clients.
                continue;
            }

            MonitorTarget &targetState = m_targetState[casefoldedNick];
            targetState.actualNick = target; // non-casefolded nick
            targetState.online = online;

            for (CClient *subscribedClient : it->second) {
                updatesToSend[subscribedClient].insert(target);
            }
        }

        const CString base_reply = CString(":irc.znc.in ") +
                                   (online ? "730" : "731") + " " + nick + " :";

        // Send updates to our ZNC clients.
        for (const auto &it : updatesToSend) {
            SplitAndSend(
                base_reply, it.second.begin(), it.second.end(), Identity,
                [&](const CString &reply) { it.first->PutClient(reply); });
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
                    if (it->second.online) {
                        cachedOnlineTargets.push_back(it->second.actualNick);
                    } else {
                        cachedOfflineTargets.push_back(it->second.actualNick);
                    }
                }
            }
        }

        // Add new monitor subscriptions, if any.
        SplitAndSend("MONITOR + ", targetsToAdd.begin(), targetsToAdd.end(),
                     Identity,
                     [&](const CString &command) { PutIRC(command); });

        // Inform client about online and offline cached targets, if any.
        const CString nick = GetNetwork()->GetIRCSock()->GetNick();
        SplitAndSend(
            ":irc.znc.in 730 " + nick + " :", cachedOnlineTargets.begin(),
            cachedOnlineTargets.end(), Identity,
            [&](const CString &reply) { m_pClient->PutClient(reply); });
        SplitAndSend(
            ":irc.znc.in 731 " + nick + " :", cachedOfflineTargets.begin(),
            cachedOfflineTargets.end(), Identity,
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
            m_targetState[targetCasefolded] = {target, false};

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
                AsActualNick(&m_targetState),
                [&](const CString &reply) { m_pClient->PutClient(reply); });
        }

        m_pClient->PutClient(":irc.znc.in 733 " +
                             GetNetwork()->GetIRCSock()->GetNick() +
                             " :End of MONITOR list");
    }

    void ClearState() {
        m_monitorLimit = -2;
        m_clientTargets.clear();
        m_targetClients.clear();
        m_targetState.clear();
    }

    // The server-side limit on the number of targets allowed.
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
};

template <> void TModInfo<CRouteMonitorMod>(CModInfo &Info) {
    Info.SetWikiPage("route_monitor");
}

NETWORKMODULEDEFS(CRouteMonitorMod,
                  "Send MONITOR status changes only to subscribed clients");
