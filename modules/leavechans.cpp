/*
 * Copyright (C) 2004-2015 ZNC, see the NOTICE file for details.
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
 *
 * Automaticaly leave chans and inform users on disconnect.
 * Author: Pierre Schweitzer <pierre@reactos.org>
 */

#include <znc/IRCNetwork.h>
#include <znc/Chan.h>
#include <znc/User.h>

class CLeaveChan {
public:
    CLeaveChan() {}

    CLeaveChan(const CString& sLine)
    {
        FromString(sLine);
    }

    CLeaveChan(const CString& sLeftChan, const CString& sNotifyChan, const CString& sNotifyMsg) :
               m_sLeftChan(sLeftChan),
               m_sNotifyChan(sNotifyChan),
               m_sNotifyMsg(sNotifyMsg) {
        m_bLeftChan = false;
    }

    virtual ~CLeaveChan() {}

    const CString& GetLeftChan() const { return m_sLeftChan; }
    const CString& GetNotifyChan() const { return m_sNotifyChan; }
    const CString& GetNotifyMsg() const { return m_sNotifyMsg; }
    bool HasBeenLeft() const { return m_bLeftChan; }
    void SetChanLeft() { m_bLeftChan = true; }
    void ResetChanLeft() { m_bLeftChan = false; }

    CString ToString() const
    {
        return m_sLeftChan + "\t" + m_sNotifyChan + "\t" + m_sNotifyMsg;
    }

    bool FromString(const CString& sLine)
    {
        m_sLeftChan = sLine.Token(0, false, "\t");
        m_sNotifyChan = sLine.Token(1, false, "\t");
        m_sNotifyMsg = sLine.Token(2, false, "\t", true);
        m_bLeftChan = false;

        return !m_sNotifyChan.empty();
    }
private:
protected:
    bool m_bLeftChan;
    CString m_sLeftChan;
    CString m_sNotifyChan;
    CString m_sNotifyMsg;
};

class CLeaveChans : public CModule {
public:
    MODCONSTRUCTOR(CLeaveChans) {}

    virtual bool OnLoad(const CString& sArgs, CString& sMessage)
    {
        for (MCString::iterator it = BeginNV(); it != EndNV(); it++)
        {
            const CString& sLine = it->second;
            CLeaveChan* pChan = new CLeaveChan;

            if (!pChan->FromString(sLine) || FindLeftChan(pChan->GetLeftChan().AsLower()))
            {
                delete pChan;
            }
            else
            {
                m_msChans[pChan->GetLeftChan().AsLower()] = pChan;
            }
        }

        return true;
    }

    virtual ~CLeaveChans()
    {
        for (std::map<CString, CLeaveChan*>::iterator it = m_msChans.begin(); it != m_msChans.end(); it++)
        {
            delete it->second;
        }

        m_msChans.clear();
    }

    virtual void OnClientDisconnect()
    {
        if (!m_pUser->IsUserAttached())
        {
            for (std::map<CString, CLeaveChan*>::const_iterator it = m_msChans.begin(); it != m_msChans.end(); it++)
            {
                const std::vector<CIRCNetwork *>& vNetworks = m_pUser->GetNetworks();
                for (std::vector<CIRCNetwork*>::const_iterator net = vNetworks.begin(); net != vNetworks.end(); ++net)
                {
                    CIRCNetwork* pNetwork = (*net);
                    CChan* pChan = pNetwork->FindChan(it->second->GetLeftChan());
                    if (pChan && pChan->IsOn())
                    {
                        PutModule("No one connected to ZNC, leaving " + it->second->GetLeftChan() + ".");
                        pNetwork->PutIRC("PART " + it->second->GetLeftChan());
                        if (it->second->GetNotifyMsg().empty())
                        {
                            pNetwork->PutIRC("PRIVMSG " + it->second->GetNotifyChan() + " :" + pNetwork->GetCurNick() + " automatically left " + it->second->GetLeftChan() + " due to client disconnection/timeout. Please check current requests.");
                        }
                        else
                        {
                            pNetwork->PutIRC("PRIVMSG " + it->second->GetNotifyChan() + " :" + m_pUser->ExpandString(it->second->GetNotifyMsg()));
                        }
                        it->second->SetChanLeft();
                        pChan->SetIsOn(false);
                        pChan->Disable();
                    }
                }
            }
        }
    }

    virtual void OnClientLogin()
    {
        for (std::map<CString, CLeaveChan*>::const_iterator it = m_msChans.begin(); it != m_msChans.end(); it++)
        {
            if (it->second->HasBeenLeft())
            {
                PutModule("You were disconnected while in " + it->second->GetLeftChan() + ". Channel has been automaticaly left.");
                it->second->ResetChanLeft();
            }
        }
    }

    virtual void OnModCommand(const CString& sLine)
    {
        CString sCommand = sLine.Token(0).AsLower();

        if (sCommand.Equals("help"))
        {
            PutModule("Automaticaly leave chans and inform users on disconnect.");
            PutModule("Commands are: About, Add, Delete, List");
        }
        else if (sCommand.Equals("about"))
        {
            PutModule("LeaveChans module version 0.2 for ZNC by Chingu <chingu@onlinegamesnet.net>.");
        }
        else if (sCommand.Equals("add"))
        {
            CString sLeftChan = sLine.Token(1);
            CString sNotifyChan = sLine.Token(2);

            if (sNotifyChan.empty())
            {
                PutModule("Usage: " + sCommand + " <channel to leave> <channel where to notify> [message]");
                PutModule("Put %nick% in notify message to display your nick.");
                return;
            }

            CLeaveChan* pChan = AddLeftChan(sLeftChan, sNotifyChan, sLine.Token(3, true, " ", true));

            if (pChan)
            {
                SetNV(sLeftChan, pChan->ToString());
            }
        }
        else if (sCommand.Equals("delete"))
        {
            CString sLeftChan = sLine.Token(1);

            if (sLeftChan.empty())
            {
                PutModule("Usage: " + sCommand + " <channel to leave>");
                return;
            }

            DelLeftChan(sLeftChan);
            DelNV(sLeftChan);
        }
        else if (sCommand.Equals("list"))
        {
            if (m_msChans.empty())
            {
                PutModule("There are no channels defined");
                return;
            }

            CTable Table;

            Table.AddColumn("Channel to leave");
            Table.AddColumn("Channel where to notify");
            Table.AddColumn("Notify message");

            for (std::map<CString, CLeaveChan*>::iterator it = m_msChans.begin(); it != m_msChans.end(); it++)
            {
                Table.AddRow();
                Table.SetCell("Channel to leave", it->second->GetLeftChan());
                Table.SetCell("Channel where to notify", it->second->GetNotifyChan());
                if (it->second->GetNotifyMsg().empty())
                {
                    Table.SetCell("Notify message", m_pUser->GetNick() + " automatically left " + it->second->GetLeftChan() + " due to client disconnection/timeout. Please check current requests.");
                }
                else
                {
                    Table.SetCell("Notify message", it->second->GetNotifyMsg());
                }
            }

            PutModule(Table);
        }
        else
        {
            PutModule("Unknown command. Try 'help' or 'about'.");
        }
    }

    CLeaveChan* FindLeftChan(const CString& sLeftChan)
    {
        std::map<CString, CLeaveChan*>::iterator it = m_msChans.find(sLeftChan.AsLower());

        return (it != m_msChans.end()) ? it->second : NULL;
    }

    CLeaveChan* AddLeftChan(const CString& sLeftChan, const CString& sNotifyChan, const CString& sNotifyMsg)
    {
        if (m_msChans.find(sLeftChan) != m_msChans.end())
        {
            PutModule("That channel already exists");
            return NULL;
        }

        CLeaveChan* pChan = new CLeaveChan(sLeftChan, sNotifyChan, sNotifyMsg);
        m_msChans[sLeftChan.AsLower()] = pChan;
        PutModule("Channel [" + sLeftChan + "] added with notify channel [" + sNotifyChan + "]");
        return pChan;
    }

    void DelLeftChan(const CString& sLeftChan)
    {
        std::map<CString, CLeaveChan*>::iterator it = m_msChans.find(sLeftChan.AsLower());

        if (it == m_msChans.end())
        {
            PutModule("That channel does not exist");
            return;
        }

        delete it->second;
        m_msChans.erase(it);
        PutModule("Channel [" + sLeftChan + "] removed");
    }
private:
    std::map<CString, CLeaveChan*> m_msChans;
};

MODULEDEFS(CLeaveChans, "Automaticaly leave chans and inform users on disconnect")
