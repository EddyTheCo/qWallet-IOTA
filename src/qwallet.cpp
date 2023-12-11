﻿#include"wallet/qwallet.hpp"
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
    {
    };
AddressBox::AddressBox(const std::shared_ptr<const Address>& addr,
                       QObject *parent,c_array outId):QObject(parent),m_addr(addr),m_amount(0),m_outId(outId)
    {
    };
std::shared_ptr<const Address> AddressBox::getAddress(void)const
{
    return m_addr;
}
QString AddressBox::getAddressHash(void)const
{
    return m_addr->addrhash().toHexString();
}
QString AddressBox::getAddressBech32(const QString hrp)const
{
    const auto addr=qencoding::qbech32::Iota::encode(hrp,m_addr->addr());
    return addr;
}
void AddressBox::monitorToExpire(const c_array outId,const quint32 unixTime)
{
    const auto triger=(unixTime-QDateTime::currentDateTime().toSecsSinceEpoch())*1000;
    QTimer::singleShot(triger,this,[=,this](){
        rmInput(outId);
    });
}
void AddressBox::monitorToUnlock(const c_array outId, const quint32 unixTime)
{

    const auto triger=(unixTime-QDateTime::currentDateTime().toSecsSinceEpoch())*1000;
    QTimer::singleShot(triger,this,[=,this](){
        auto resp=NodeConnection::instance()->mqtt()->get_outputs_outputId(outId.toHexString());
        connect(resp,&ResponseMqtt::returned,this,[=,this](QJsonValue data){
            auto node_outputs=std::vector<Node_output>{Node_output(data)};
            getOutputs({node_outputs});
            resp->deleteLater();
        });
    });
}
void AddressBox::monitorToSpend(const c_array outId)
{

    auto resp=NodeConnection::instance()->mqtt()->get_outputs_outputId(outId.toHexString());
    connect(resp,&ResponseMqtt::returned,this,[=,this](QJsonValue data){
        const auto node_output=Node_output(data);
        if(node_output.metadata().is_spent_)
        {
            resp->deleteLater();
            rmInput(outId);
        }

    });
}
void AddressBox::addInput(const c_array outId, const InBox inBox)
{
    m_inputs.insert(outId,inBox);
    emit inputAdded(outId);

    if(inBox.output->type()==Output::NFT_typ||inBox.output->type()==Output::Alias_typ)
    {
        AddressBox* nextAddr=nullptr;
        if(inBox.output->type()==Output::NFT_typ)
        {
            nextAddr=new AddressBox(Address::NFT(inBox.output->get_id()),this,outId);
        }
        if(inBox.output->type()==Output::Alias_typ)
        {
            nextAddr=new AddressBox(Address::Alias(inBox.output->get_id()),this,outId);

        }
        addAddrBox(outId,nextAddr);
    }
    setAmount(m_amount+inBox.amount);
}
void AddressBox::rmInput(const c_array outId)
{
    if(m_inputs.contains(outId))
    {
        const auto var=m_inputs.take(outId);

        if(var.output->type()==Output::NFT_typ||var.output->type()==Output::Alias_typ)
        {
            rmAddrBox(outId,var.amount);
        }
        else
        {
            emit inputRemoved(outId);

            setAmount(m_amount-var.amount);
        }

    }

}
void AddressBox::addAddrBox(const c_array outId,AddressBox* addrBox)
{

    connect(addrBox,&AddressBox::amountChanged,this,[this](const auto prevA,const auto nextA){
        setAmount(m_amount-prevA+nextA);
    });
    m_AddrBoxes.insert(outId,addrBox);
    emit addrAdded(addrBox);
}
void  AddressBox::clean()
{
    while(!m_inputs.empty())
    {
        rmInput(m_inputs.begin().key());
    }
    deleteLater();
}
void AddressBox::rmAddrBox(const c_array outId,const quint64 outputAmount)
{
    const auto var=m_AddrBoxes.take(outId);
    const auto address=var->getAddress()->addr();
    emit addrRemoved(address);

    connect(var,&QObject::destroyed,this,[=,this]{
        emit inputRemoved(outId);
        setAmount(m_amount-outputAmount);
    });
    var->clean();

}
pvector<const Unlock> AddressBox::getUnlocks(const QByteArray & message, const quint16 &ref, const size_t& inputSize)
{
    pvector<const Unlock> unlocks;
    for(size_t i=0;i<inputSize;i++)
    {
        if(m_addr->type()==Address::Ed25519_typ)
        {
            if(unlocks.size())
            {
                unlocks.push_back(Unlock::Reference(ref));
            }
            else
            {
                unlocks.push_back(Unlock::Signature(Signature::Ed25519(m_keyPair.first,qcrypto::qed25519::sign(m_keyPair,message))));
            }
        }
        if(m_addr->type()==qblocks::Address::Alias_typ)
        {
            unlocks.push_back(Unlock::Alias(ref));
        }
        if(m_addr->type()==qblocks::Address::NFT_typ)
        {
            unlocks.push_back(Unlock::NFT(ref));
        }
    }
    return unlocks;
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
                    monitorToUnlock(v.metadata().outputid_,unix_time);
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
                    monitorToExpire(v.metadata().outputid_,unix_time);
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
            monitorToSpend(v.metadata().outputid_);
            addInput(v.metadata().outputid_,inBox);

        }
        outs_.pop_back();
    }
}


Wallet::Wallet(QObject *parent):QObject(parent),m_amount(0),accountIndex(0),addressRange(1)
{
    connect(NodeConnection::instance(),&NodeConnection::stateChanged,this,&Wallet::reset);
    connect(Account::instance(),&Account::Changed,this,&Wallet::reset);
}
Wallet* Wallet::instance()
{
    if (!m_instance) m_instance=new Wallet();
    return m_instance;
}
void Wallet::reset(void)
{
    if(NodeConnection::instance()->state()==NodeConnection::Connected)
    {
        m_instance=new Wallet(Wallet::instance()->parent());
        emit ready();
        m_instance->sync();
        deleteLater();
    }
}
void Wallet::sync(void)
{
    for(quint32 pub=0;pub<1;pub++)
    {
        for (quint32 i=0;i<addressRange;i++)
        {
            auto addressBundle = new AddressBox(Account::instance()->getKeys({accountIndex,pub,i}),
                                                Wallet::instance());

            checkAddress(addressBundle);
            connect(addressBundle,&AddressBox::amountChanged,this,[this](auto prevA,auto nextA){
                m_amount=m_amount-prevA + nextA;
                emit amountChanged();
            });
        }
    }
    emit synced();

}

void Wallet::checkAddress(AddressBox  *addressBundle)
{
    addAddress(addressBundle);
    auto info=NodeConnection::instance()->rest()->get_api_core_v2_info();
    connect(info,&Node_info::finished,this,[=,this]( ){
        const auto addressBech32=addressBundle->getAddressBech32(info->bech32Hrp);

        auto nodeOutputs=NodeConnection::instance()->rest()->
                           get_outputs<Output::All_typ>("unlockableByAddress="+addressBech32);

        connect(nodeOutputs,&Node_outputs::finished,addressBundle,[=,this]( ){
            checkOutputs(nodeOutputs->outs_,addressBundle);
            nodeOutputs->deleteLater();
            auto resp=NodeConnection::instance()->mqtt()->
                        get_outputs_unlock_condition_address("address/"+addressBech32);
            connect(resp,&ResponseMqtt::returned,addressBundle,[=,this](QJsonValue data)
                    {
                        if(m_addresses.contains(addressBundle->getAddress()->addr()))
                            checkOutputs({Node_output(data)},addressBundle);
                        else
                            resp->deleteLater();
                    });
        });
        connect(addressBundle,&QObject::destroyed,nodeOutputs,&QObject::deleteLater);
        info->deleteLater();
    });
}
void Wallet::addAddress(AddressBox  * addressBundle)
{
    const auto outid=addressBundle->outId();

    connect(addressBundle,&AddressBox::inputAdded,this,[=,this](c_array id){
        auto newVec=m_outputs.contains(outid)?m_outputs[outid]:std::vector<std::pair<AddressBox*,c_array>>{};
        newVec.push_back(std::make_pair(addressBundle,id));
        m_outputs[id]=newVec;
    });
    connect(addressBundle,&AddressBox::inputRemoved,this,[this](const c_array id){
        m_outputs.erase(id);
        usedOutIds.erase(id);
    });
    const auto serialAddress=addressBundle->getAddress()->addr();
    m_addresses[serialAddress]=addressBundle;
    emit addressesChanged(serialAddress);
    connect(addressBundle,&AddressBox::addrAdded,this,[=,this](AddressBox* addr){
        checkAddress(addr);
    });
    connect(addressBundle,&AddressBox::addrRemoved,this,[=,this](c_array seriAddress){
        m_addresses.erase(seriAddress);
    });
}
void Wallet::checkOutputs(std::vector<Node_output> outs,AddressBox* addressBundle)
{
    addressBundle->getOutputs(outs);
}
quint64 Wallet::consumeInbox(const QString & outId,const InBox & inBox,
                             StateOutputs &stateOutputs)const
{
    if(inBox.output->type()!=Output::Basic_typ)
    {
        stateOutputs.insert(outId,inBox);
    }
    return inBox.amount;
}

quint64 Wallet::consumeInputs(const c_array & outId,
                              InputSet& inputSet,StateOutputs& stateOutputs)
{
    quint64 amount=0;

    if(m_outputs.contains(outId)&&!usedOutIds.contains(m_outputs[outId].front().second))
    {

        std::vector<std::pair<AddressBox*,std::set<c_array>>>::iterator it;
        for(const auto& v:m_outputs[outId] )
        {
            bool addrExist=false;
            for(it=inputSet.begin(); it != inputSet.end(); it++)
            {
                if(it->first->getAddressHash()==v.first->getAddressHash())
                {
                    addrExist=true;
                    if(!it->second.contains(v.second))
                    {
                        amount+=consumeInbox(v.second,v.first->inputs().value(v.second),stateOutputs);
                        it->second.insert(v.second);
                    }
                    break;
                }
            }
            if(!addrExist)
            {
                if(v.first->outId().isEmpty())
                {
                    inputSet.push_back(std::make_pair(v.first,std::set<c_array>{v.second}));
                }
                else
                {
                    inputSet.insert(it,std::make_pair(v.first,std::set<c_array>{v.second}));
                }
                amount+=consumeInbox(v.second,v.first->inputs().value(v.second),stateOutputs);
            }

        }

    }
    return amount;
}
quint64 Wallet::consume(InputSet& inputSet, StateOutputs &stateOutputs,
                        const quint64 &amountNeedIt,
                        const std::set<Output::types>& onlyType,
                        const std::set<c_array> &outids)
{

    quint64 amount=0;

    for(const auto&v:outids)
    {
        if(onlyType.contains(Output::All_typ)||
            (m_outputs.contains(v)&&m_outputs[v].back().first&&
             onlyType.contains(m_outputs[v].back().first->inputs().value(v).output->type())))
            amount+=consumeInputs(v,inputSet,stateOutputs);
    }

    for(const auto & [key,value]:m_outputs)
    {
        if(amount>=amountNeedIt&&amountNeedIt)
            break;
        if(onlyType.contains(Output::All_typ)||
            (m_outputs.contains(key)&&m_outputs[key].back().first&&m_outputs[key].back().first->inputs().value(key).output&&
             onlyType.contains(m_outputs[key].back().first->inputs().value(key).output->type())))
            amount+=consumeInputs(key,inputSet,stateOutputs);
    }
    return amount;
}

pvector<const Unlock> Wallet::createUnlocks(const InputSet& inputSet,const qblocks::c_array& essenceHash)const
{
    pvector<const Unlock> theUnlocks;
    quint16 ref=0;
    std::pair<AddressBox*,std::set<c_array>> prev;
    for(const auto& v:inputSet)
    {
        const auto outIds=v.second;
        const auto addressBox=v.first;

        auto varRef=ref;
        if(!addressBox->outId().isEmpty())
        {
            const auto posi=std::distance(prev.second.find(addressBox->outId()),prev.second.end());
            varRef-=posi;
        }
        const auto unlocks=v.first->getUnlocks(essenceHash,varRef,v.second.size());
        theUnlocks.insert(theUnlocks.end(), unlocks.begin(), unlocks.end());

        ref+=outIds.size();
        prev=v;
    }
    return theUnlocks;
}
std::pair<std::shared_ptr<const Payload>,std::set<QString>>
Wallet::createTransaction(const InputSet& inputSet,Node_info* info, const pvector<const Output>& outputs)
{
    pvector<const Input> inputs;
    qblocks::c_array InputsHash;
    pvector<const Output> theOutputs=outputs;
    std::set<QString> usedIds;
    for(const auto& v:inputSet)
    {

        for(const auto& outId:v.second)
        {
            usedIds.insert(outId);
            usedOutIds.insert(outId);
            const auto inBox=v.first->inputs().value(outId);
            inputs.push_back(inBox.input);
            InputsHash+=inBox.inputHash;
            if(inBox.retOutput)theOutputs.push_back(inBox.retOutput);
        }

    }
    auto InputsCommitment=Block::get_inputs_Commitment(InputsHash);
    auto essence=Essence::Transaction(info->network_id_,inputs,InputsCommitment,theOutputs);
    const auto essenceHash=essence->get_hash();
    const auto unlocks=createUnlocks(inputSet,essenceHash);
    return std::make_pair(Payload::Transaction(essence,unlocks),usedIds);
}

}

