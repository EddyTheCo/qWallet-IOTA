#include <QCoreApplication>
#include"wallet/qwallet.hpp"
#include"nodeConnection.hpp"
#include"account.hpp"
#include<QTimer>
#include<QJsonDocument>

using namespace qiota;


int main(int argc, char** argv)
{

    auto a=QCoreApplication(argc, argv);


    // It is mandatory to pass the node address and the seed to the application
    if(argc>1)
    {
        qDebug()<<"entered";

        NodeConnection::instance()->setNodeAddr(QUrl(argv[1]));
        Account::instance()->setSeed(argv[2]);
        if(argc>2)NodeConnection::instance()->rest()->JWT=QString(argv[3]);


        QObject::connect(Wallet::instance(),&Wallet::ready,&a,[&a](){
            QObject::connect(Wallet::instance(),&Wallet::amountChanged,&a,[&a](){
                qDebug()<<"Amount on the wallet:"<<Wallet::instance()->amount();
                QObject::disconnect(Wallet::instance(),&Wallet::amountChanged,&a,nullptr);
                auto info=NodeConnection::instance()->rest()->get_api_core_v2_info();

                QObject::connect(info,&Node_info::finished,&a,[=,&a]( ){
                    qDebug()<<"Hello";
                    InputSet inputSet;
                    StateOutputs stateOutputs;

                    qDebug()<<"Hello:"<<Wallet::instance()->addresses().size();
                    const auto address=Wallet::instance()->addresses().begin().value()->getAddress();


                    const auto sendFea=Feature::Sender(address);
                    const auto addUnlCon=Unlock_Condition::Address(address);
                    auto BaOut= Output::Basic(0,{addUnlCon},{},{sendFea});

                    const auto deposit=Client::get_deposit(BaOut,info);
                    const auto minDeposit=Client::get_deposit(Output::Basic(0,{addUnlCon}),info);
                    auto consumedAmount=Wallet::instance()->consume(inputSet,stateOutputs,deposit);

                    qDebug()<<"consumedAmount:"<<consumedAmount;
                    qDebug()<<"deposit:"<<deposit;
                    BaOut->amount_=deposit;
                    pvector<const Output> theOutputs{BaOut};

                    if(consumedAmount>=deposit)
                    {

                        if(consumedAmount-deposit!=0)
                        {
                            theOutputs.push_back(Output::Basic(consumedAmount-deposit,{addUnlCon},{},{}));
                        }
                        //Because we are not adding `stateOutputs` to `theOutputs` we are burnig them, Foundry outputs can not be burn so easy.
                        auto payloadusedids=Wallet::instance()->createTransaction(inputSet,info,theOutputs);
                        auto block=Block(payloadusedids.first);
                        const auto transactionid=payloadusedids.first->get_id().toHexString();
                        auto res=NodeConnection::instance()->mqtt()->get_subscription("transactions/"+transactionid +"/included-block");
                        QObject::connect(res,&ResponseMqtt::returned,&a,[=,&a](auto var){
                            qDebug()<<"The block is confirmed by a milestone";
                            qDebug()<<"Transaction Id:"<<transactionid;
                        });

                        // Send the block to the node
                        qDebug().noquote()<<"block:\n"<<QString(QJsonDocument(block.get_Json()).toJson(QJsonDocument::Indented));
                        NodeConnection::instance()->rest()->send_block(block);

                    }

                    info->deleteLater();
                });

            });
        });

        /*auto info=NodeConnection::instance()->rest()->get_api_core_v2_info();
        QObject::connect(info,&Node_info::finished,&a,[=]( ){
            const auto address=Account::instance()->getAddrBech32({0,0,0},info->bech32Hrp);
            qDebug()<<"Asking funds for "<<address;
            //NodeConnection::instance()->rest()->getFundsFromFaucet(address);
            info->deleteLater();
        });
*/
        return a.exec();
    }


}
