#include "ctp_market_spi.h"

namespace co {

    CTPMarketSpi::CTPMarketSpi(CThostFtdcMdApi* qApi) {
        api_ = qApi;
    }

    CTPMarketSpi::~CTPMarketSpi() {

    }

    ///���ͻ����뽻�׺�̨������ͨ������ʱ����δ��¼ǰ�����÷��������á�
    void CTPMarketSpi::OnFrontConnected() {
        __info << "connect to CTP market server ok, ApiVersion: " << api_->GetApiVersion();
        ReqUserLogin();
    };

    ///���ͻ����뽻�׺�̨ͨ�����ӶϿ�ʱ���÷��������á���������������API���Զ��������ӣ��ͻ��˿ɲ�������
    ///@param nReason ����ԭ��
    ///        0x1001 �����ʧ��
    ///        0x1002 ����дʧ��
    ///        0x2001 ����������ʱ
    ///        0x2002 ��������ʧ��
    ///        0x2003 �յ�������
    void CTPMarketSpi::OnFrontDisconnected(int nReason) {
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
        x::Sleep(10000); // sleep 10s������CTP�ײ����Ͻ���������
    };

    ///��¼������Ӧ
    void CTPMarketSpi::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
        if (pRspInfo->ErrorID == 0) {
            login_trading_day_ = atol(api_->GetTradingDay());
            local_date_ = x::RawDate();
            __info << "OnRspUserLogin, login_trading_day = " << login_trading_day_ << ", local_date_: " << local_date_;
            ReqSubMarketData();
        } else {
            __error << "login failed: ret=" << pRspInfo->ErrorID << ", msg=" << ctp_str(pRspInfo->ErrorMsg);
        }
    };

    ///��������Ӧ��
    void CTPMarketSpi::OnRspSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
        if (!pSpecificInstrument) {
            return;
        }
        if (!pRspInfo) {
            return;
        }
        string code = pSpecificInstrument->InstrumentID;
        bool sub_ok = pRspInfo->ErrorID == 0 ? true : false;
        sub_status_[code] = sub_ok;
        int success = 0;
        int failure = 0;
        for (auto p : sub_status_) {
            if (p.second == 1) {
                success++;
            } else if (p.second == -1) {
                failure++;
            }
        }
        int count = success + failure;
        if (sub_ok) {
            __info << "[" << count << "/" << sub_status_.size() << "] sub ok: code = " << code;
        } else {
            __error << "[" << count << "/" << sub_status_.size() << "] sub failed: code = "
                << code << ", error = " << pRspInfo->ErrorID << "-" << ctp_str(pRspInfo->ErrorMsg);
        }
        if (count >= (int)sub_status_.size()) {
            __info << "sub quotation over, total: " << sub_status_.size() << ", success: " << success << ", failure: " << failure;
            __info << "server startup successfully";
        }
    };

    //void CTPMarketSpi::OnRspQryMulticastInstrument(CThostFtdcMulticastInstrumentField* pMulticastInstrument, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast) {
    //    __info << "OnRspQryMulticastInstrument, InstrumentID: " << pMulticastInstrument->InstrumentID
    //        << ", InstrumentNo: " << pMulticastInstrument->InstrumentID
    //        << ", TopicID: " << pMulticastInstrument->TopicID
    //        << ", reserve1: " << pMulticastInstrument->reserve1
    //        << ", CodePrice: " << pMulticastInstrument->CodePrice
    //        << ", VolumeMultiple: " << pMulticastInstrument->VolumeMultiple
    //        << ", PriceTick: " << pMulticastInstrument->PriceTick;
    //}

    ///�������֪ͨ
    void CTPMarketSpi::OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *p) {
        if (!p) {
            return;
        }
        string ctp_code = p->InstrumentID;
        // ������TradingDay����ActionDay, ��ͬ�������ı仯û�й���
        QContextPtr ctx = QServer::Instance()->GetContext(ctp_code);
        if (!ctx) {
            __warn << "no context of " << ctp_code;
            return;
        }
        int64_t date = atoi(p->TradingDay); // 20170101, ���Է��֣�ż�������TradingDayΪ�յ������Ҳ�����յ�ǰһ�������
        if (date > 0 && date < local_date_) {
            __warn << "ignore data with unexpected date: code = " << ctp_code << ", TradingDay = " << date << ", local_date = " << local_date_;
            return;
        }
        int64_t action_date = atoi(p->ActionDay);
        string s = p->UpdateTime; // 15:00:01
        boost::algorithm::replace_all(s, ":", "");
        int64_t stamp = atoll(s.c_str()) * 1000LL + p->UpdateMillisec;

        bool first_flag = false;
        auto it = first_flag_.find(ctp_code);
        if (it == first_flag_.end()) {
            first_flag = true;
            __info << "monitor info, code: " << ctp_code
                << ", ExchangeID: " << p->ExchangeID
                << ", UpdateTime: " << p->UpdateTime;
        }

        bool not_valid_flag = false;
        if (stamp < 0 || stamp > 235959999) {
            not_valid_flag = true;
        }

        // ����6:00 �� 8:40
        if (stamp > 55959999 && stamp < 83959999) {
            not_valid_flag = true;
        }

        // ����16:00 �� 20:40
        if (stamp > 155959999 && stamp < 203959999) {
            not_valid_flag = true;
        }

        // ÿ����Լ�ĵ�һ�ʲ��ж�����Ч��, �����ʹ��
        if (not_valid_flag && !first_flag) {
            __warn << "ignore data with unexpected time: code = " << ctp_code << ", UpdateTime = " << p->UpdateTime << ", UpdateMillisec = " << p->UpdateMillisec;
            return;
        }

        co::fbs::QTickT& _m = ctx->tick();
        int64_t pre_tick_date = _m.timestamp / 1000000000LL;
        int64_t pre_stamp = _m.timestamp % 1000000000LL;
        int64_t timestamp = pre_tick_date * 1000000000LL + stamp;
        //ÿ����Լ�����ж� ��23��59���ɵ�00��00, ǰһ��ʱ��� > 20:00, �µ�ʱ��� < 06:00
        if (pre_stamp > 195959999 && stamp < 55959999) {
            int64_t new_date = x::NextDay(pre_tick_date);
            timestamp = new_date * 1000000000LL + stamp;
            __info << " change date, pre_stamp: " << pre_stamp << ", now stamp: " << stamp
                   << ", pre_tick_date: " << pre_tick_date << ", new_date: " << new_date;
        }

        int64_t delta_ms = x::SubRawDateTime(timestamp, x::RawDateTime());
        if (delta_ms > 60000) { // �����һ�����ݵ�ʱ��ȵ�ǰϵͳʱ�仹Ҫ��60�������ϣ���������
            __warn << "drop data: InstrumentID = " << p->InstrumentID << ", timestamp = " << timestamp
                << ", TradingDay: " << p->TradingDay << ", ActionDay: " << p->ActionDay;
            return;
        }

//        if (p->InstrumentID[0] == 's' && p->InstrumentID[1] == 'i') {
//            __info << "OnRtnDepthMarketData, InstrumentID: " << p->InstrumentID
//                << ", ExchangeID: " << p->ExchangeID
//                << ", UpdateTime: " << p->UpdateTime
//                << ", BidVolume1: " << p->BidVolume1
//                << ", BidVolume2: " << p->BidVolume2
//                << ", BidVolume3: " << p->BidVolume3
//                << ", BidVolume4: " << p->BidVolume4
//                << ", BidVolume5: " << p->BidVolume5
//                << ", BidPrice1: " << p->BidPrice1
//                << ", BidPrice2: " << p->BidPrice1
//                << ", BidPrice3: " << p->BidPrice1
//                << ", BidPrice4: " << p->BidPrice1
//                << ", BidPrice5: " << p->BidPrice1
//                << ", AskVolume1: " << p->AskVolume1
//                << ", AskVolume2: " << p->AskVolume2
//                << ", AskVolume3: " << p->AskVolume3
//                << ", AskVolume4: " << p->AskVolume4
//                << ", AskVolume5: " << p->AskVolume5
//                << ", AskPrice1: " << p->AskPrice1
//                << ", AskPrice2: " << p->AskPrice1
//                << ", AskPrice3: " << p->AskPrice1
//                << ", AskPrice4: " << p->AskPrice1
//                << ", AskPrice5: " << p->AskPrice1;
//        }

        co::fbs::QTickT& m = ctx->PrepareQTick();
        m.timestamp = timestamp;
        m.pre_close = ctp_price(p->PreClosePrice);
        m.upper_limit = ctp_price(p->UpperLimitPrice);
        m.lower_limit = ctp_price(p->LowerLimitPrice);
        while (true) {
            if (p->BidVolume1 > 0) {
                m.bp.emplace_back(ctp_price(p->BidPrice1));
                m.bv.emplace_back(p->BidVolume1);
            } else {
                break;
            }
            if (p->BidVolume2 > 0) {
                m.bp.emplace_back(ctp_price(p->BidPrice2));
                m.bv.emplace_back(p->BidVolume2);
            } else {
                break;
            }
            if (p->BidVolume3 > 0) {
                m.bp.emplace_back(ctp_price(p->BidPrice3));
                m.bv.emplace_back(p->BidVolume3);
            } else {
                break;
            }
            if (p->BidVolume4 > 0) {
                m.bp.emplace_back(ctp_price(p->BidPrice4));
                m.bv.emplace_back(p->BidVolume4);
            } else {
                break;
            }
            if (p->BidVolume5 > 0) {
                m.bp.emplace_back(ctp_price(p->BidPrice5));
                m.bv.emplace_back(p->BidVolume5);
            } else {
                break;
            }
            break;
        }
        while (true) {
            if (p->AskVolume1 > 0) {
                m.ap.emplace_back(ctp_price(p->AskPrice1));
                m.av.emplace_back(p->AskVolume1);
            } else {
                break;
            }
            if (p->AskVolume2 > 0) {
                m.ap.emplace_back(ctp_price(p->AskPrice2));
                m.av.emplace_back(p->AskVolume2);
            } else {
                break;
            }
            if (p->AskVolume3 > 0) {
                m.ap.emplace_back(ctp_price(p->AskPrice3));
                m.av.emplace_back(p->AskVolume3);
            } else {
                break;
            }
            if (p->AskVolume4 > 0) {
                m.ap.emplace_back(ctp_price(p->AskPrice4));
                m.av.emplace_back(p->AskVolume4);
            } else {
                break;
            }
            if (p->AskVolume5 > 0) {
                m.ap.emplace_back(ctp_price(p->AskPrice5));
                m.av.emplace_back(p->AskVolume5);
            } else {
                break;
            }
            break;
        }
        // ֣�������ɽ������Ҫ���Ժ�Լ����
        int64_t sum_volume = p->Volume;
        double sum_amount = m.market == co::kMarketCZCE && m.multiple > 0 ? p->Turnover * m.multiple : p->Turnover;
        m.new_price = ctp_price(p->LastPrice);

        // ��Ч��Tick
        if (not_valid_flag) {
            m.new_volume = 0;
            m.new_amount = 0;
        } else {
            if (first_flag) {
                // ��Ч�ĵ�һ��Tick, new_volume �� new_amount����Ϊ 0
                __info << "first tick, code: " << ctp_code
                    << ", ExchangeID: " << p->ExchangeID
                    << ", UpdateTime: " << p->UpdateTime;
                m.new_volume = 0;
                m.new_amount = 0;
                first_flag_.insert(ctp_code);
            } else {
                m.new_volume = sum_volume - m.sum_volume;
                m.new_amount = sum_amount - m.sum_amount;
            }
        }

        m.sum_volume = sum_volume;
        m.sum_amount = sum_amount;
        m.open = ctp_price(p->OpenPrice);
        m.high = ctp_price(p->HighestPrice);
        m.low = ctp_price(p->LowestPrice);
        m.open_interest = (int64_t)p->OpenInterest;
        m.pre_settle = ctp_price(p->PreSettlementPrice);
        m.pre_open_interest = (int64_t)p->PreOpenInterest;
        m.close = ctp_price(p->ClosePrice);
        m.settle = ctp_price(p->SettlementPrice);
        string line = ctx->FinishQTick();
        QServer::Instance()->PushQTick(line);
        //__info << "on data: " << m->ShortDebugString();
    };

    ///����Ӧ��
    void CTPMarketSpi::OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
        __error << "OnRspError: ret=" << pRspInfo->ErrorID << ", msg=" << ctp_str(pRspInfo->ErrorMsg);
    };

    int CTPMarketSpi::next_id() {
        return ctp_request_id_++;
    }

    void CTPMarketSpi::ReqUserLogin() {
        __info << "login ...";
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

    void CTPMarketSpi::ReqSubMarketData() {
        __info << "sub quotation ...";
        sub_status_.clear();
        const map<string, QContextPtr>& contracts = QServer::Instance()->contexts();
        vector<string> sub_codes;
        for (auto pair : contracts) {
            string ctp_code = pair.first; // key�ǲ�����׺�ĺ�Լ����
            sub_status_[ctp_code] = false;
            sub_codes.emplace_back(ctp_code);
        }
        size_t limit = 100; // ÿ����ඩ��100������
        for (size_t i = 0; i < sub_codes.size(); ) {
            size_t size = i + limit <= sub_codes.size() ? limit : sub_codes.size() - i;
            char** pCodes = new char*[size];
            memset(pCodes, 0, sizeof(char*) * size);
            for (size_t j = i, k = 0; j < i + size; j++, k++) {
                pCodes[k] = (char *)sub_codes[j].c_str();
            }
            int rc = 0;
            while ((rc = api_->SubscribeMarketData(pCodes, size)) != 0) {
                __warn << "SubscribeMarketData failed: ret=" << rc << ", retring ...";
                x::Sleep(kCtpRetrySleepMs);
            }
            delete[] pCodes;
            i += size;
        }
    }
}

