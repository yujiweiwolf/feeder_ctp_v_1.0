#include "ctp_trade_spi.h"
#include <regex>

namespace co {

    CTPTradeSpi::CTPTradeSpi(CThostFtdcTraderApi* api) {
        api_ = api;
    }

    CTPTradeSpi::~CTPTradeSpi() {

    }

    ///当客户端与交易后台建立起通信连接时（还未登录前），该方法被调用。
    void CTPTradeSpi::OnFrontConnected() {
        __info << "connect to CTP trade server ok";
        // 在登陆之前，服务端要求对客户端进行身份认证，客户端通过认证之后才能请求登录。期货公司可以关闭该功能。
        string ctp_app_id = Config::Instance()->ctp_app_id();
        if (!ctp_app_id.empty()) {
            ReqAuthenticate();
        } else {
            ReqUserLogin();
        }
    };

    ///当客户端与交易后台通信连接断开时，该方法被调用。当发生这个情况后，API会自动重新连接，客户端可不做处理。
    ///@param nReason 错误原因
    ///        0x1001 网络读失败
    ///        0x1002 网络写失败
    ///        0x2001 接收心跳超时
    ///        0x2002 发送心跳失败
    ///        0x2003 收到错误报文
    void CTPTradeSpi::OnFrontDisconnected(int nReason) {
        stringstream ss;
        ss << "ret=" << nReason << ", msg=";
        switch (nReason) {
            case 0x1001:
                ss << "read error";
                break;
            case 0x1002:
                ss << "write error";
                break;
            case 0x2001:
                ss << "recv heartbeat timeout";
                break;
            case 0x2002:
                ss << "send heartbeat timeout";
                break;
            case 0x2003:
                ss << "recv broken data";
                break;
            default:
                ss << "unknown error";
                break;
        }
        __info << "connection is broken: " << ss.str();
        x::Sleep(1000);
    };

    void CTPTradeSpi::ReqAuthenticate() {
        __info << "authenticate ...";
        string broker_id = Config::Instance()->ctp_broker_id();
        string investor_id = Config::Instance()->ctp_investor_id();
        string app_id = Config::Instance()->ctp_app_id();
        string product_info = Config::Instance()->ctp_product_info();
        string auth_code = Config::Instance()->ctp_auth_code();
        if (app_id.length() >= sizeof(TThostFtdcAppIDType)) {
            __fatal << "app_id is too long, max length is " << (sizeof(TThostFtdcAppIDType) - 1);
            xthrow() << "app_id is too long, max length is " << (sizeof(TThostFtdcAppIDType) - 1);
        }
        if (product_info.length() >= sizeof(TThostFtdcProductInfoType)) {
            __fatal << "product_info is too long, max length is " << (sizeof(TThostFtdcProductInfoType) - 1);
            xthrow() << "product_info is too long, max length is " << (sizeof(TThostFtdcProductInfoType) - 1);
        }
        if (auth_code.length() >= sizeof(TThostFtdcAuthCodeType)) {
            __fatal << "auth_code is too long, max length is " << (sizeof(TThostFtdcAuthCodeType) - 1);
            xthrow() << "auth_code is too long, max length is " << (sizeof(TThostFtdcAuthCodeType) - 1);
        }
        CThostFtdcReqAuthenticateField req;
        memset(&req, 0, sizeof(req));
        strcpy(req.BrokerID, broker_id.c_str());
        strcpy(req.UserID, investor_id.c_str());
        strcpy(req.AppID, app_id.c_str());
        strcpy(req.UserProductInfo, product_info.c_str());
        strcpy(req.AuthCode, auth_code.c_str());
        int rc = 0;
        while ((rc = api_->ReqAuthenticate(&req, next_id())) != 0) {
            __warn << "ReqAuthenticate failed: " << rc << ", retring ...";
            x::Sleep(kCtpRetrySleepMs);
        }
    }

    /// 客户端认证响应
    void CTPTradeSpi::OnRspAuthenticate(CThostFtdcRspAuthenticateField *pRspAuthenticateField, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
        if (pRspInfo == NULL || pRspInfo->ErrorID == 0) {
            __info << "authenticate ok";
            ReqUserLogin();
        } else {
            __error << "authenticate failed: ret=" << pRspInfo->ErrorID << ", msg=" << ctp_str(pRspInfo->ErrorMsg);
        }
    }

    void CTPTradeSpi::ReqUserLogin() {
        __info << "login ...";
        cur_req_type_ = "ReqUserLogin";
        CThostFtdcReqUserLoginField req;
        memset(&req, 0, sizeof(req));
        strcpy(req.BrokerID, Config::Instance()->ctp_broker_id().c_str());
        strcpy(req.UserID, Config::Instance()->ctp_investor_id().c_str());
        strcpy(req.Password, Config::Instance()->ctp_password().c_str());
        int rc = 0;
        while ((rc = api_->ReqUserLogin(&req, next_id())) != 0) {
            __warn << "ReqUserLogin failed: ret=" << rc << ", retring ...";
            x::Sleep(kCtpRetrySleepMs);
        }
    }

    ///登录请求响应
    void CTPTradeSpi::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
        if (pRspInfo->ErrorID == 0) {
            date_ = atoi(api_->GetTradingDay());
            __info << "trade login ok: trading_day = " << date_;
            if (date_ < 19700101 || date_ > 29991231) {
                __error << "illegal trading day: " << date_;
                over_ = true;
            } else if (date_ < x::RawDate()) {
                __error << "trading day is not today: trading_day=" << date_ << ", today=" << x::RawDate() << ", quit ...";
                over_ = true;
            } else {
                // 如果立即查询合约，可能会在OnRspError中收到报错：ret=90, msg=CTP：查询未就绪，请稍后重试
                x::Sleep(5000);
                ReqQryInstrument();
            }
        } else {
            __error << "login failed: ret=" << pRspInfo->ErrorID << ", msg=" << ctp_str(pRspInfo->ErrorMsg);
            over_ = true;
        }
    };

    void CTPTradeSpi::OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
        if (pRspInfo->ErrorID == 0) {
            __info << "logout ok";
        } else {
            __error << "logout failed: ret=" << pRspInfo->ErrorID << ", msg=" << ctp_str(pRspInfo->ErrorMsg);
        }
    }

    void CTPTradeSpi::ReqQryInstrument() {
        __info << "query contracts ...";
        cur_req_type_ = "ReqQryInstrument";
        CThostFtdcQryInstrumentField req;
        memset(&req, 0, sizeof(req));
        int rc = 0;
        while ((rc = api_->ReqQryInstrument(&req, next_id())) != 0) {
            __warn << "ReqQryInstrument failed: ret=" << rc << ", retring ...";
            x::Sleep(kCtpRetrySleepMs);
        }
    }

    string InsertCzceCode(const string ctp_code) {
        string code =ctp_code;
        int index = 0;
        for (unsigned int i = 0; i < code.length(); i++) {
            char temp = code.at(i);
            if (temp >= '0' && temp <= '9') {
                code.insert(index, "2");
                break;
            } else {
                index++;
            }
        }
        return code;
    }

    ///请求查询合约响应
    void CTPTradeSpi::OnRspQryInstrument(CThostFtdcInstrumentField *p, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
        if (pRspInfo == NULL || pRspInfo->ErrorID == 0) {
            string ctp_code = p->InstrumentID;
            int64_t market = ctp_market2std(p->ExchangeID);
            string suffix = co::Market2Suffix(market);
            string std_code;
            if (market == co::kMarketCZCE) {
                std_code = InsertCzceCode(ctp_code) + suffix;
                // LOG_INFO << "fix code, " << ctp_code << ", std_code: " << std_code;
            } else {
                std_code = ctp_code + suffix;
            }
            int status = p->IsTrading ? kStateOK : kStateSuspension;
            // 去掉组合合约
            bool is_future = (p->ProductClass == THOST_FTDC_PC_Futures);
            bool is_option = (p->ProductClass == THOST_FTDC_PC_Options || p->ProductClass == THOST_FTDC_PC_SpotOption);
            bool enable_future = QServer::Instance()->options()->enable_future();
            bool enable_option = QServer::Instance()->options()->enable_option();
            if ((is_future && enable_future) || (is_option && enable_option)) {
                QContextPtr ctx = QServer::Instance()->NewContext(ctp_code, std_code);
                co::fbs::QTickT& m = ctx->PrepareQTick();
                m.src = kSrcQTickLevel2;
                m.dtype = is_future ? kDTypeFuture : kDTypeOption;
                m.date = date_;
                m.timestamp = x::RawDate() * 1000000000;
                m.code = std_code;
                m.name = ctp_str(p->InstrumentName);
                m.market = market;
                m.status = status;
                m.multiple = p->VolumeMultiple >= 0 ? p->VolumeMultiple : 0;
                m.price_step = p->PriceTick;
                m.create_date = atoi(p->CreateDate);
                m.list_date = atoi(p->OpenDate);
                m.expire_date = atoi(p->ExpireDate);
                m.start_settle_date = atoi(p->StartDelivDate);
                m.end_settle_date = atoi(p->EndDelivDate);
                if (is_option) {
                    m.exercise_date = m.expire_date;
                    m.exercise_price = p->StrikePrice;
                    if (market == co::kMarketCZCE) {
                        m.underlying_code = InsertCzceCode(x::Trim(p->UnderlyingInstrID)) + suffix;
                    } else {
                        m.underlying_code = x::Trim(p->UnderlyingInstrID) + suffix;
                    }
                    m.cp_flag = (p->OptionsType == THOST_FTDC_CP_CallOptions) ? kCpFlagCall : kCpFlagPut;
                }
                m.new_volume = -1; // 设置new_volume为-1用来
                if (is_option) {
                    LOG_INFO << "Option, " << m.code << ", cp_flag: " << (int)m.cp_flag << ", underlying_code: " << m.underlying_code
                             << ", ExchangeID: " << p->ExchangeID << ", market: " << market;
                }
            }
            if (bIsLast) {
                map<string, QContextPtr>& contexts = QServer::Instance()->contexts();
                vector<string> ctxs;
                for (auto itr : contexts) {
                    ctxs.push_back(itr.second->tick().code);
                }
                std::sort(ctxs.begin(), ctxs.end(), [&](const string & a, const string & b) -> bool {return a < b; });
                stringstream ss;
                ss << "query contracts ok: size=" << ctxs.size() << ", codes=";
                for (size_t i = 0; i < ctxs.size(); ++i) {
                    if (i > 0) {
                        ss << ",";
                    }
                    ss << ctxs[i];
                }
                __info << ss.str();
                over_ = true;
            }
        } else {
            __error << "query all future codes failed: ret=" << pRspInfo->ErrorID << ", msg=" << ctp_str(pRspInfo->ErrorMsg);
            over_ = true;
        }
    }

    ///错误应答
    void CTPTradeSpi::OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
        __error << "OnRspError: ret=" << pRspInfo->ErrorID << ", msg=" << ctp_str(pRspInfo->ErrorMsg);
        if (pRspInfo->ErrorID == 90) { // <error id="NEED_RETRY" value="90" prompt="CTP：查询未就绪，请稍后重试" />
            x::Sleep(kCtpRetrySleepMs);
            if (cur_req_type_ == "ReqUserLogin") {
                ReqUserLogin();
            } else if (cur_req_type_ == "ReqQryInstrument") {
                ReqQryInstrument();
            }
        }
    };

    void CTPTradeSpi::Wait() {
        while (!over_) {
            x::Sleep(10); // sleep 10ms
        }
    }

    int CTPTradeSpi::next_id() {
        return ctp_request_id_++;
    }


}

