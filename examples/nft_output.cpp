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
            QObject::connect(Wallet::instance(),&Wallet::addressesChanged,&a,[=,&a](auto addr){
                qDebug()<<"Wallet has new address:"<<addr;
            });
            auto info=NodeConnection::instance()->rest()->get_api_core_v2_info();

            QObject::connect(info,&Node_info::finished,&a,[=,&a]( ){

                QObject::connect(Wallet::instance(),&Wallet::amountChanged,&a,[=,&a](){
                    qDebug()<<"Amount on the wallet:"<<Wallet::instance()->amount();
                    for(auto v:Wallet::instance()->addresses())
                    {
                        qDebug()<<v->getAddressBech32(info->bech32Hrp);
                    }
                    static auto sent{false};
                    const auto address=Account::instance()->getAddr({0,0,0}); //We need to order the addresses on the wallet
                    const auto sendFea=Feature::Sender(address);
                    const auto issuFea=Feature::Issuer(address);
                    const auto imMetFea=Feature::Metadata("WENN? SOON BUT IMMUTABLE");
                    const auto tagFea=Feature::Tag("from IOTA-Qt");
                    const auto metFea=Feature::Metadata("WENN? SOON");
                    const auto addUnlCon=Unlock_Condition::Address(address);

                    auto NFTOut= Output::NFT(0,{addUnlCon},{},
                                              {imMetFea,issuFea},
                                              {sendFea,tagFea,metFea});

                    const auto deposit=Client::get_deposit(NFTOut,info);
                    const auto minDeposit=Client::get_deposit(Output::Basic(0,{addUnlCon}),info);

                    InputSet inputSet;
                    StateOutputs stateOutputs;

                    auto consumedAmount=Wallet::instance()->consume(inputSet,stateOutputs,deposit,{Output::Basic_typ},
                    {"0xb9167ff3d3bd39d6b1beacebe54070a3ed7d31ffcea8b95760230b95e5d3de320000"});
                    NFTOut->amount_=deposit;
                    pvector<const Output> theOutputs{NFTOut};

                    quint64 stateAmount=0;
                    for(const auto &v:std::as_const(stateOutputs))
                    {
                        stateAmount+=v.amount;
                        theOutputs.push_back(v.output);
                    }
                    qDebug()<<"consumedAmount:"<<consumedAmount;
                    qDebug()<<"deposit:"<<deposit;
                    qDebug()<<"stateAmount:"<<stateAmount;
                    qDebug()<<"sent:"<<sent;
                    const quint64 requiredAmount=deposit+stateAmount;

                    if(consumedAmount>=requiredAmount&&!sent)
                    {
                        sent=true;
                        if(consumedAmount-requiredAmount!=0)
                        {
                            if(consumedAmount-requiredAmount>=minDeposit)
                                theOutputs.push_back(Output::Basic(consumedAmount-requiredAmount,{addUnlCon},{},{}));
                            else
                                NFTOut->amount_+=consumedAmount-requiredAmount;
                        }
                        auto payloadUsedIds=Wallet::instance()->createTransaction(inputSet,info,theOutputs);
                        auto block=Block(payloadUsedIds.first);
                        const auto transactionId=payloadUsedIds.first->get_id().toHexString();
                        auto res=NodeConnection::instance()->mqtt()->get_subscription("transactions/"+transactionId +"/included-block");
                        QObject::connect(res,&ResponseMqtt::returned,&a,[=,&a](auto var){
                            qDebug()<<"The block is confirmed by a milestone";
                            qDebug()<<"Transaction Id:"<<transactionId;
                        });

                        // Send the block to the node
                        qDebug().noquote()<<"block:\n"<<QString(QJsonDocument(block.get_Json()).toJson(QJsonDocument::Indented));
                        NodeConnection::instance()->rest()->send_block(block);

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


