#include"qwallet.hpp"
#include"account.hpp"
#include"nodeConnection.hpp"
#include<QTimer>
namespace qiota{

using namespace qblocks;

Wallet* Wallet::m_instance=nullptr;


Wallet::Wallet(QObject *parent):QObject(parent),m_amount(0),accountIndex(0),addressRange(1)
#if defined(USE_QML)
    ,m_amountJson(new Qml64(m_amount,this))
#endif
{
#if defined(USE_QML)
    connect(this, &Wallet::amountChanged, m_amountJson,[=]()
            {m_amountJson->setValue(m_amount);});
#endif
    connect(NodeConnection::instance(),&NodeConnection::stateChanged,this,&Wallet::reset);
    connect(Account::instance(),&Account::changed,this,&Wallet::reset);
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
        for(const auto&[k,v]:m_addresses)
        {
            v->deleteLater();
        }
        m_addresses.clear();
        m_outputs.clear();
        usedOutIds.clear();
#if defined(USE_QML)
        m_amountJson->setValue(0);
#endif
        m_amount=0;
        m_instance->sync();
    }
}
void Wallet::sync(void)
{
    auto info=NodeConnection::instance()->rest()->get_api_core_v2_info();
    connect(info,&Node_info::finished,this,[=]( ){
        for(quint32 pub=0;pub<1;pub++)
        {
            for (quint32 i=0;i<addressRange;i++)
            {
                auto addressBundle = new AddressBox(Account::instance()->getKeys({accountIndex,pub,i}),info->bech32Hrp,this);

                checkAddress(addressBundle);
                connect(addressBundle,&AddressBox::amountChanged,this,[this](auto prevA,auto nextA){
                    m_amount=m_amount-prevA + nextA;
                    emit amountChanged();
                });
            }
        }
        emit synced();
        info->deleteLater();
    });

}

void Wallet::checkAddress(AddressBox  *addressBundle)
{
    addAddress(addressBundle);
    const auto addressBech32=addressBundle->getAddressBech32();

    auto nodeOutputs=NodeConnection::instance()->rest()->
                       get_outputs<Output::All_typ>("unlockableByAddress="+addressBech32);

    connect(nodeOutputs,&Node_outputs::finished,addressBundle,[=]( ){
        checkOutputs(nodeOutputs->outs_,addressBundle);
        nodeOutputs->deleteLater();
        auto resp=NodeConnection::instance()->mqtt()->
                    get_outputs_unlock_condition_address("address/"+addressBech32);
        connect(resp,&ResponseMqtt::returned,addressBundle,[=](QJsonValue data)
                {
                    checkOutputs({Node_output(data)},addressBundle);
                });
    });
    connect(addressBundle,&QObject::destroyed,nodeOutputs,&QObject::deleteLater);

}
void Wallet::addAddress(AddressBox  * addressBundle)
{
    connect(addressBundle,&AddressBox::inputRemoved,this,[=](const auto ids){
        for(const auto& id: ids)
        {
            m_outputs.erase(id);
            usedOutIds.erase(id);
            emit inputRemoved(id);
        }

    });
    connect(addressBundle,&AddressBox::addrRemoved,this,[=](const auto addrs){
        for(const auto& addr: addrs)
        {
            m_addresses.erase(addr);
        }

    });
    connect(addressBundle,&AddressBox::addrAdded,this,[=](AddressBox* addr){
        checkAddress(addr);
    });


    const auto outid=addressBundle->outId();

    connect(addressBundle,&AddressBox::inputAdded,this,[=](c_array id){
        auto newVec=std::vector<std::pair<AddressBox*,c_array>>{};

        if(m_outputs.find(outid)!=m_outputs.end()) newVec=m_outputs.at(outid);
        newVec.push_back(std::make_pair(addressBundle,id));
        m_outputs[id]=newVec;
        emit inputAdded(id);
    });

    const auto serialAddress=addressBundle->getAddress()->addr();
    m_addresses[serialAddress]=addressBundle;
    emit addressesChanged(serialAddress);


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
    if((m_outputs.find(outId)!=m_outputs.cend())&&
        (usedOutIds.find(m_outputs[outId].front().second)==usedOutIds.cend()))
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
                    if((it->second.find(v.second)==it->second.cend()))
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
    auto ts = (onlyType.find(Output::All_typ)!=onlyType.cend());
    for(const auto&v:outids)
    {
        auto os = ((m_outputs.find(v)!=m_outputs.cend())&&
                   (onlyType.find(m_outputs.at(v).back().first->inputs().value(v).output->type())!=onlyType.cend()));

        if( ts || os)
            amount+=consumeInputs(v,inputSet,stateOutputs);
    }

    for(const auto & [v,value]:m_outputs)
    {
        if(amount>=amountNeedIt&&amountNeedIt)
            break;


        auto os = ((m_outputs.find(v)!=m_outputs.cend())&&
                   (onlyType.find(m_outputs.at(v).back().first->inputs().value(v).output->type())!=onlyType.cend()));
        if( ts || os)
            amount+=consumeInputs(v,inputSet,stateOutputs);
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

std::vector<AddressBox*> Wallet::getAddresses()
{
    std::vector<AddressBox*> vec;
    for(const auto& [key,value]:m_addresses) {
        vec.push_back(value);
    }
    return vec;
}
}

