#pragma once
#include"client/qnode_outputs.hpp"
#include"client/qnode_info.hpp"
#include <QObject>

#include<set>
#include <QtCore/QtGlobal>
#if defined(WINDOWS_QWALLET)
# define QWALLET_EXPORT Q_DECL_EXPORT
#else
#define QWALLET_EXPORT Q_DECL_IMPORT
#endif

namespace qiota{

using namespace qblocks;


struct InBox {
    std::shared_ptr<const Input> input=nullptr;
    std::shared_ptr<const Output> retOutput=nullptr;
    qblocks::c_array inputHash;
    std::shared_ptr<Output> output=nullptr;
    quint64 amount=0;
};
class QWALLET_EXPORT AddressBox:public QObject
{
    Q_OBJECT

public:
    AddressBox(const std::pair<QByteArray,QByteArray>& keyPair,QObject *parent = nullptr);

    AddressBox(const std::shared_ptr<const Address>& addr,QObject *parent = nullptr, QString outId=QString());

    std::shared_ptr<const Address> getAddress(void)const;
    QString getAddressBech32(const QString hrp)const;
    QString getAddressHash(void)const;

    void getOutputs(std::vector<Node_output> &outs, const quint64 amountNeedIt=0, const quint16 howMany=0);
    pvector<const Unlock> getUnlocks(const QByteArray & message, const quint16 &ref, const std::set<QString>& outIds);
    quint64 amount(void)const{return m_amount;};
    const QHash<QString,AddressBox*> addrBoxes(void)const{return m_AddrBoxes;};
    const QHash<QString,InBox> inputs(void)const{return m_inputs;};
    QString outId()const{return m_outId;}

signals:
    void amountChanged(quint64 prevA,quint64 nextA);
    void changed();
    void inputRemoved(QString);
    void inputAdded(QString);
    void addrAdded(AddressBox*);
    void addrRemoved(AddressBox*);

private:
    void clean();
    void monitorToSpend(const QString& outId);
    void monitorToExpire(const QString& outId,const quint32 unixTime);
    void monitorToUnlock(const QString& outId,const quint32 unixTime);
    void rmInput(const QString outId);
    void addInput(const QString& outId,const InBox& inBox);
    void rmAddrBox(const QString& outId);
    void addAddrBox(const QString& outId,AddressBox* addrBox);

    void setAmount(const quint64 amount){
        if(amount!=m_amount){emit amountChanged(m_amount,amount);m_amount=amount;}}

    QHash<QString,InBox> m_inputs;
    QHash<QString,AddressBox*> m_AddrBoxes;
    quint64 m_amount;

    const std::pair<QByteArray,QByteArray> m_keyPair;
    std::shared_ptr<const Address> m_addr;
    QString m_outId;

};
using InputSet = std::vector<std::pair<AddressBox*,std::set<QString>>>;
using StateOutputs=QHash<QString,InBox>;
using InputMap=QHash<QString,std::vector<std::pair<AddressBox*,QString>>>;
class QWALLET_EXPORT Wallet: public QObject
{
    Q_OBJECT
    Wallet(QObject *parent = nullptr);

public:
    static Wallet* instance();

    auto amount(void)const{return m_amount;};
    auto addresses()const{return m_addresses;}
    auto inputs()const{return m_outputs;}
    quint64 consume(InputSet& inputSet, StateOutputs &stateOutputs,
                    const quint64& amountNeedIt=0,
                    const std::set<Output::types>& onlyType={Output::All_typ},
                    const std::set<QString> &outids={});
    std::pair<std::shared_ptr<const Payload>,std::set<QString>> createTransaction
        (const InputSet &inputSet, Node_info* info, const pvector<const Output> &outputs);
    void checkOutputs(std::vector<Node_output>  outs, AddressBox *addressBundle);
signals:
    void addressesChanged(QString);
    void amountChanged();
    void ready();
    void synced();
private:
    void sync(void);
    void reset(void);
    pvector<const Unlock> createUnlocks(const InputSet& inputSet, const c_array &essenceHash)const;
    quint64 consumeInputs(const QString & outId, InputSet &inputSet, StateOutputs &stateOutputs);
    quint64 consumeInbox(const QString & outId,const InBox & inBox, StateOutputs &stateOutputs)const;
    bool getInput(const QString& input,InputSet& inputSet);
    void checkAddress(AddressBox *addressBundle);
    quint64 m_amount;
    quint32 accountIndex, addressRange;
    InputMap m_outputs; //use better a map for ordering the inputs to consume them
    std::set<QString> usedOutIds;
    QHash<QString,AddressBox*> m_addresses; //use better a map to order the address to use mostly the 0 index ed25519
    static Wallet * m_instance;


};

}
