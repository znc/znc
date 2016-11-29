/*
 * Copyright (C) 2004-2016 ZNC, see the NOTICE file for details.
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

#include <znc/Modules.h>
#include <znc/Client.h>
#include <znc/User.h>
#include <znc/znc.h>

#include "../third_party/QR-Code-generator/cpp/BitBuffer.cpp"
#include "../third_party/QR-Code-generator/cpp/QrCode.cpp"
#include "../third_party/QR-Code-generator/cpp/QrSegment.cpp"
#include "../third_party/cppcodec/cppcodec/base32_rfc4648.hpp"

#include <liboath/oath.h>

class COtpAuth : public CAuthBase {
    std::shared_ptr<CAuthBase> m_parent;
    CString m_sOtp;
    CString m_sSecret;

  public:
    COtpAuth(std::shared_ptr<CAuthBase> parent, const CString& sPass,
             const CString& sOtp, const CString& sSecret)
        : CAuthBase(parent->GetUsername(), sPass,
                    static_cast<CZNCSock*>(parent->GetSocket())),
          m_parent(parent),
          m_sOtp(sOtp),
          m_sSecret(sSecret) {}

    void Invalidate() override {
        m_parent->Invalidate();
        CAuthBase::Invalidate();
    }

    void AcceptedLogin(CUser& User) override {
        int iCheck = oath_totp_validate(
            m_sSecret.c_str(), m_sSecret.length(), time(nullptr),
            OATH_TOTP_DEFAULT_TIME_STEP_SIZE, OATH_TOTP_DEFAULT_START_TIME,
            /* window = */ 1, m_sOtp.c_str());
        if (iCheck < 0) {
            m_parent->RefuseLogin("OTP error: " +
                                  CString(oath_strerror(iCheck)));
        } else {
            m_parent->AcceptLogin(User);
        }
    }

    void RefusedLogin(const CString& sReason) override {
        m_parent->RefuseLogin(sReason);
    }
};

class COtpAuthMod : public CModule {

  public:
    MODCONSTRUCTOR(COtpAuthMod) {}

    CString GetWebMenuTitle() override { return "OTP"; }
    bool OnWebRequest(CWebSock& WebSock, const CString& sPageName,
                      CTemplate& Tmpl) override {
        CString sUser = WebSock.GetSession()->GetUser()->GetUserName();
        if (sPageName == "index") {
            Tmpl["exists"] = CString(HasNV(sUser));
            return true;
        }
        if (sPageName == "regen" && WebSock.IsPost()) {
            using qrcodegen::QrCode;
            CString sURL = GenerateSecretURL(sUser);
            Tmpl["otpurl"] = sURL;
            QrCode code = QrCode::encodeText(sURL.c_str(), QrCode::Ecc::MEDIUM);
            CString sSVG = code.toSvgString(/* border = */ 2);
            // Remove xml/doctype headers
            sSVG = sSVG.substr(sSVG.find("<svg") + 4);
            sSVG = "<svg style='height: 100%'" + sSVG;
            Tmpl["qr"] = sSVG;
            return true;
        }
        if (sPageName == "clear" && WebSock.IsPost()) {
            DelNV(sUser);
            WebSock.Redirect(GetWebPath());
            return true;
        }
        return false;
    }

    CString GenerateSecretURL(const CString& sUser) {
        CString sSecret = CString::RandomString(30);
        SetNV(sUser, sSecret);
        return "otpauth://totp/" + sUser + "?secret=" +
               cppcodec::base32_rfc4648::encode(sSecret) + "&issuer=ZNC";
    }

    // TODO: redo this properly, with SASL SECURID mechanism.
    EModRet OnLoginAttempt(std::shared_ptr<CAuthBase> Auth) override {
        if (!HasNV(Auth->GetUsername())) {
            return CONTINUE;
        }
        if (dynamic_cast<COtpAuth*>(Auth.get()) != nullptr) {
            return CONTINUE;
        }
        constexpr int uOtpLen = 6;
        if (Auth->GetPassword().length() <= uOtpLen) {
            return CONTINUE;
        }
        CString sOtp = Auth->GetPassword().Right(uOtpLen);
        CString sPass = Auth->GetPassword().RightChomp_n(uOtpLen);
        CZNC::Get().AuthUser(std::make_shared<COtpAuth>(
            Auth, sPass, sOtp, GetNV(Auth->GetUsername())));
        return HALT;
    }
};

template <>
void TModInfo<COtpAuthMod>(CModInfo& Info) {
    Info.SetWikiPage("otpauth");
}

GLOBALMODULEDEFS(COtpAuthMod, "")
