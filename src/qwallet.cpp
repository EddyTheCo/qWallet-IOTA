#include"wallet/qwallet.hpp"
#include"account.hpp"
#include"nodeConnection.hpp"
#include<QTimer>
namespace qiota{

using namespace qblocks;

Wallet* Wallet::m_instance=nullptr;
AddressBox::AddressBox(const std::pair<QByteArray,QByteArray>& keyPair,QObject *parent)
    :QObject(parent),m_keyPair(keyPair),
    m_addr(std::shared_ptr<Address>(new Ed25519_Address(
        QCryptographicHash::hash(keyPair.first,QCryptographicHash::Blake2b_256)))),
    m_amount(0)
    { };
AddressBox::AddressBox(const std::shared_ptr<const Address>& addr,
                       QObject *parent,QString outId):QObject(parent),m_addr(addr),m_amount(0),m_outId(outId)
    { };

std::shared_ptr<const Address> AddressBox::getAddress(void)const
{
    return m_addr;
}
QString AddressBox::getAddressBech32(const QString hrp)const
{
    const auto addr=qencoding::qbech32::Iota::encode(hrp,getAddress()->addr());
    return addr;
}
void AddressBox::monitorToExpire(const QString& outId,const quint32 unixTime)
{
    const auto triger=(unixTime-QDateTime::currentDateTime().toSecsSinceEpoch())*1000;
    QTimer::singleShot(triger,this,[=](){
        rmInput(outId);
    });
}
void AddressBox::monitorToUnlock(const QString& outId,const quint32 unixTime)
{
    const auto triger=(unixTime-QDateTime::currentDateTime().toSecsSinceEpoch())*1000;
    QTimer::singleShot(triger,this,[=](){
        auto resp=Node_Conection::instance()->mqtt()->get_outputs_outputId(outId);
        connect(resp,&ResponseMqtt::returned,this,[=](QJsonValue data){
            auto node_outputs=std::vector<Node_output>{Node_output(data)};
            getOutputs({node_outputs});
            resp->deleteLater();
        });
    });
}
void AddressBox::monitorToSpend(const QString& outId)
{
    auto resp=Node_Conection::instance()->mqtt()->get_outputs_outputId(outId);
    connect(resp,&ResponseMqtt::returned,this,[=](QJsonValue data){
        const auto node_output=Node_output(data);
        if(node_output.metadata().is_spent_)
        {
            resp->deleteLater();
            rmInput(outId);
        }

    });
}
void AddressBox::addInput(const QString& outId,const InBox& inBox)
{
    emit inputAdded(outId);
    setAmount(m_amount+inBox.amount);
    m_inputs.insert(outId,inBox);
    if(inBox.output->type()==Output::NFT_typ)
        addAddrBox(outId,new AddressBox(Address::NFT(inBox.output->get_id()),this,outId));
    if(inBox.output->type()==Output::Alias_typ)
        addAddrBox(outId,new AddressBox(Address::Alias(inBox.output->get_id()),this,outId));
}
void AddressBox::rmInput(const QString& outId)
{
    emit inputRemoved(outId);
    const auto var=m_inputs.take(outId);
    setAmount(m_amount-var.amount);
    if(var.output->type()==Output::NFT_typ||var.output->type()==Output::Alias_typ)
        rmAddrBox(outId);


}
void AddressBox::addAddrBox(const QString& outId,AddressBox* addrBox)
{
    emit addrAdded(outId);
    connect(addrBox,&AddressBox::amountChanged,this,[=](const auto prevA,const auto nextA){
        setAmount(m_amount-prevA+nextA);
    });
    m_AddrBoxes.insert(outId,addrBox);
}
void AddressBox::rmAddrBox(const QString& outId)
{
    emit addrRemoved(outId);
    const auto var=m_AddrBoxes.take(outId);
    setAmount(m_amount-var->amount());
    var->deleteLater();
}

void AddressBox::getOutputs(std::vector<Node_output> &outs_, const quint64 amount_need_it, quint16 howMany)
{

    const auto size=outs_.size();
    while(((amount_need_it)?m_amount<amount_need_it:true)&&((howMany>=size)||(!howMany)?!outs_.empty():outs_.size()+howMany>size))
    {
        const auto v=outs_.back();
        if(!v.metadata().is_spent_&&!(m_inputs.contains(v.metadata().outputid_)))
        {
            const auto output_=v.output();

            const auto  stor_unlock=output_->get_unlock_(Unlock_Condition::Storage_Deposit_Return_typ);
            quint64 retAmount=0;
            std::shared_ptr<const Output> retOut=nullptr;
            if(stor_unlock)
            {
                const auto sdruc=std::static_pointer_cast<const Storage_Deposit_Return_Unlock_Condition>(stor_unlock);
                retAmount=sdruc->return_amount();
                const auto ret_address=sdruc->address();
                const auto retUnlcon = Unlock_Condition::Address(ret_address);
                retOut = Output::Basic(retAmount,{retUnlcon});
            }
            const auto cday=QDateTime::currentDateTime().toSecsSinceEpoch();
            const auto time_lock=output_->get_unlock_(Unlock_Condition::Timelock_typ);
            if(time_lock)
            {
                const auto time_lock_cond=std::static_pointer_cast<const Timelock_Unlock_Condition>(time_lock);
                const auto unix_time=time_lock_cond->unix_time();
                if(cday<unix_time)
                {
                    monitorToUnlock(v.metadata().outputid_.toHexString(),unix_time);
                    outs_.pop_back();
                    retOut=nullptr;
                    continue;
                }
            }
            const auto expir=output_->get_unlock_(Unlock_Condition::Expiration_typ);
            if(expir)
            {
                const auto expiration_cond=std::static_pointer_cast<const Expiration_Unlock_Condition>(expir);
                const auto unix_time=expiration_cond->unix_time();
                const auto ret_address=expiration_cond->address();

                const auto  addr_unlock=std::static_pointer_cast<const Address_Unlock_Condition>(output_->get_unlock_(Unlock_Condition::Address_typ));
                if(ret_address->addr()==getAddress()->addr()&&addr_unlock->address()->addr()!=getAddress()->addr())
                {
                    retOut=nullptr;
                    retAmount=0;

                    if(cday<=unix_time)
                    {
                        outs_.pop_back();
                        continue;
                    }
                }
                else
                {
                    if(cday>unix_time)
                    {
                        retOut=nullptr;
                        outs_.pop_back();
                        continue;
                    }
                    monitorToExpire(v.metadata().outputid_.toHexString(),unix_time);
                }

            }

            InBox inBox;
            inBox.input=Input::UTXO(v.metadata().transaction_id_,v.metadata().output_index_);

            qblocks::c_array prevOutputSer;
            prevOutputSer.from_object<Output>(*v.output());
            inBox.inputHash=QCryptographicHash::hash(prevOutputSer, QCryptographicHash::Blake2b_256);

            if(output_->type()!=Output::Basic_typ)
            {
                if(output_->type()!=Output::Foundry_typ&&output_->get_id()==c_array(32,0))
                {
                    output_->set_id(v.metadata().outputid_);
                }
            }
            inBox.output=output_;
            inBox.amount+=output_->amount_-retAmount;
            monitorToSpend(v.metadata().outputid_.toHexString());
            addInput(v.metadata().outputid_.toHexString(),inBox);

        }
        outs_.pop_back();
    }
}


Wallet::Wallet(QObject *parent):QObject(parent),m_amount(0),accountIndex(0),addressRange(10)
{

}
Wallet* Wallet::instance()
{
    if (!m_instance) m_instance=new Wallet();
    return m_instance;
}
void Wallet::sync(void)
{
    if(Node_Conection::instance()->state()==Node_Conection::Connected)
    {
        m_instance=new Wallet(Wallet::instance()->parent());
        for(quint32 pub=0;pub<2;pub++)
        {
            for (quint32 i=0;i<addressRange;i++)
            {
                auto addressBundle = new AddressBox(Account::instance()->getKeys({accountIndex,pub,i}),Wallet::instance());

                connect(addressBundle,&AddressBox::amountChanged,this,[=](auto prevA,auto nextA){
                    m_amount=m_amount-prevA + nextA;
                    emit amountChanged();
                });
                checkAddress(addressBundle);
            }
        }
        deleteLater();
    }
}
void Wallet::checkAddress(AddressBox  *addressBundle)
{
    auto info=Node_Conection::instance()->rest()->get_api_core_v2_info();
    connect(info,&Node_info::finished,this,[=]( ){
        const auto bech32address=addressBundle->getAddressBech32(info->bech32Hrp);
        m_addresses.insert(bech32address,addressBundle);
        connect(addressBundle,&QObject::destroyed,this,[=](){
            m_addresses.remove(bech32address);
        });
        const auto addressBech32=addressBundle->getAddressBech32(info->bech32Hrp);
        auto node_outputs_=new Node_outputs();
        connect(node_outputs_,&Node_outputs::finished,addressBundle,[=]( ){
            checkOutputs(node_outputs_->outs_,addressBundle);
            node_outputs_->deleteLater();
            auto resp=Node_Conection::instance()->mqtt()->
                        get_outputs_unlock_condition_address("address/"+addressBech32);
            connect(resp,&ResponseMqtt::returned,addressBundle,[=](QJsonValue data)
                    {
                        checkOutputs({Node_output(data)},addressBundle);
                    });
        });
        Node_Conection::instance()->rest()->
            get_outputs<Output::All_typ>(node_outputs_,"unlockableByAddress="+addressBech32);
        info->deleteLater();
    });
}

void Wallet::checkOutputs(std::vector<Node_output> outs,AddressBox* addressBundle)
{
    addressBundle->getOutputs(outs);
    const auto outid=addressBundle->outId();
    const auto prevVec=m_outputs.value(outid);
    for (auto i = addressBundle->inputs().cbegin(), end = addressBundle->inputs().cend(); i != end; ++i)
    {
        auto newVec=prevVec;
        newVec.push_back(std::make_pair(addressBundle,i.key()));
        m_outputs.insert(i.key(),newVec);
    }
    connect(addressBundle,&AddressBox::inputAdded,[=](auto id){
        auto newVec=prevVec;
        newVec.push_back(std::make_pair(addressBundle,id));
        m_outputs.insert(id,newVec);
    });
    connect(addressBundle,&AddressBox::inputRemoved,[=](auto id){
        m_outputs.remove(id);
    });
    for (auto v:addressBundle->addrBoxes())
    {
        checkAddress(v);
    }
    connect(addressBundle,&AddressBox::addrAdded,[=](auto id){
        checkAddress(addressBundle->addrBoxes().value(id));
    });
}

}
