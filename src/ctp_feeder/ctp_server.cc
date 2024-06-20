#include "ctp_server.h"

namespace co {

    void CTPServer::Run() {
        running_ = true;
        QOptionsPtr opt = Config::Instance()->opt();
        QServer::Instance()->Init(opt);
        QueryContracts();
        auto& ctxs = QServer::Instance()->contexts();
        std::map<std::string, QContextPtr> contexts; // ���ݺ�Լ��Ϣ
        for (auto itr : ctxs) {
            contexts[itr.first] = itr.second;
        }
        if (contexts.empty()) {
            __error << "none contracts got from CTP server, couldn't sub quotation, now quit ...";
            return;
        }
        auto ctx = contexts.begin()->second;
        auto& tick = ctx->tick();
        int64_t trading_day = tick.date; // �ӵ�һ����Լ��̬��Ϣ�л�ȡ��ǰ������
        __info << "Set QServer trading_day: " << trading_day;
        QServer::Instance()->Start(); // ��������������contexts
        for (auto itr : contexts) { // �������ú�Լ
            QServer::Instance()->SetContext(itr.first, itr.second);
        }
        SubQuotation();
        QServer::Instance()->Wait();
        Stop();
        QServer::Instance()->Stop();
    };

    void CTPServer::QueryContracts() {
        // ͨ�����׽ӿڲ�ѯ�����ڻ���Լ
        __info << "connecting to ctp trade server ...";
        CThostFtdcTraderApi* tApi = CThostFtdcTraderApi::CreateFtdcTraderApi("");
        CTPTradeSpi* tSpi = new CTPTradeSpi(tApi);
        tApi->RegisterSpi(tSpi);
        string trade_addr = Config::Instance()->ctp_trade_front();
        tApi->RegisterFront((char*)trade_addr.c_str());
        tApi->SubscribePublicTopic(THOST_TERT_RESTART);
        tApi->SubscribePrivateTopic(THOST_TERT_RESUME);
        tApi->Init();
        tSpi->Wait(); // �ȴ���ѯ����
        tApi->RegisterSpi(nullptr);
        tApi->Release();
        tApi = nullptr;
        delete tSpi;
        tSpi = nullptr;
    }

    void CTPServer::SubQuotation() {
        __info << "connecting to ctp market server ...";
        qApi_ = CThostFtdcMdApi::CreateFtdcMdApi("");
        qSpi_ = new CTPMarketSpi(qApi_);
        qApi_->RegisterSpi(qSpi_);
        string market_addr = Config::Instance()->ctp_market_front();
        qApi_->RegisterFront((char*)market_addr.c_str());
        qApi_->Init();
    }

    void CTPServer::Stop() {
        running_ = false;
        if (qApi_) {
            qApi_->RegisterSpi(nullptr);
            qApi_->Release();
            qApi_ = nullptr;
        }
        if (qSpi_) {
            delete qSpi_;
            qSpi_ = nullptr;
        }
    }

}

