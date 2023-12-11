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

        NodeConnection::instance()->setNodeAddr(QUrl(argv[1]));
        if(argc>2)NodeConnection::instance()->rest()->JWT=QString(argv[3]);
        Account::instance()->setSeed(argv[2]);

        QObject::connect(Wallet::instance(),&Wallet::ready,&a,[=,&a](){

            qDebug()<<"Wallet::ready";
            QObject::connect(Wallet::instance(),&Wallet::addressesChanged,&a,[=,&a](c_array addr){
                qDebug()<<"Wallet added or removed address:"<<qencoding::qbech32::Iota::encode("rms",addr);
            });
            auto info=NodeConnection::instance()->rest()->get_api_core_v2_info();

            QObject::connect(info,&Node_info::finished,&a,[=,&a]( ){
                QObject::connect(Wallet::instance(),&Wallet::amountChanged,&a,[=,&a](){
                    qDebug()<<"Amount on the wallet:"<<Wallet::instance()->amount();

                    for(const auto& [key,value]:Wallet::instance()->addresses())
                    {
                        qDebug()<<"address:"<<qencoding::qbech32::Iota::encode("rms",key);
                        qDebug()<<"amount:"<<value->amount();
                        for (auto i = value->inputs().cbegin(), end = value->inputs().cend(); i != end; ++i)
                        {
                            qDebug()<<"\t input:" << i.key().toHexString();
                            qDebug()<<"\t amount:"<<i.value().amount;
                        }
                    }
                    qDebug()<<"inputs:";
                    for(const auto& [key,value]:Wallet::instance()->inputs())
                    {
                        qDebug()<<key.toHexString();
                        for(const auto &v: value)
                        {
                            qDebug()<<"\t address:"<<v.first->getAddressBech32("rms");
                            qDebug()<<"\t outid: "<<v.second.toHexString();
                        }
                    }

                   const auto serialaddress=qencoding::qbech32::Iota::decode("rms1zrwzpry0aaplncyhh9snq2vplfgu0mk754rc9ha52c8qnp87xseuj8mus4j").second;
                    if(Wallet::instance()->addresses().contains(serialaddress))
                    {
                        static auto sent{false};
                        const auto addB=Wallet::instance()->addresses()[serialaddress];
                        const auto parentOutId=addB->outId();
                        qDebug()<<"parentOutId:"<<parentOutId.toHexString();
                        const auto address=addB->getAddress();
                        const auto sendFea=Feature::Sender(address);
                        const auto addUnlCon=Unlock_Condition::Address(address);
                        auto BaOut = Output::Basic(0,{addUnlCon},{},{sendFea});
                        const auto deposit=Client::get_deposit(BaOut,info);
                        const auto minDeposit=Client::get_deposit(Output::Basic(0,{addUnlCon}),info);

                        InputSet inputSet;
                        StateOutputs stateOutputs;
                        auto consumedAmount=Wallet::instance()->
                                              consume(inputSet,stateOutputs,deposit,{Output::All_typ},{parentOutId});

                        BaOut->amount_=deposit;
                        pvector<const Output> theOutputs{BaOut};

                        quint64 stateAmount=0;
                        for(const auto &v:std::as_const(stateOutputs))
                        {
                            stateAmount+=v.amount;
                        }
                        qDebug()<<"consumedAmount:"<<consumedAmount;
                        qDebug()<<"deposit:"<<deposit;
                        qDebug()<<"stateAmount:"<<stateAmount;
                        qDebug()<<"sent:"<<sent;

                        quint64 requiredAmount=deposit+stateAmount;

                        if(consumedAmount<requiredAmount)
                        {
                            consumedAmount+=Wallet::instance()->
                                              consume(inputSet,stateOutputs,0,{Output::Basic_typ},{});
                        }

                        stateAmount=0;
                        for(const auto &v:std::as_const(stateOutputs))
                        {
                            stateAmount+=v.amount;
                            theOutputs.push_back(v.output);
                        }
                        qDebug()<<"Try again";
                        qDebug()<<"consumedAmount:"<<consumedAmount;
                        qDebug()<<"deposit:"<<deposit;
                        qDebug()<<"stateAmount:"<<stateAmount;
                        qDebug()<<"sent:"<<sent;
                        requiredAmount=deposit+stateAmount;
                        if(consumedAmount>=requiredAmount&&!sent)
                        {
                            sent=true;
                            if(consumedAmount-requiredAmount!=0)
                            {
                                    BaOut->amount_+=consumedAmount-requiredAmount;
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

                    }

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
