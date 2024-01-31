#pragma once
#include "qaddr_bundle.hpp"
#include "block/qpayloads.hpp"
#include "client/qnode_info.hpp"

#if defined(USE_QML)
#include<QtQml>
#endif

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


using InputSet = std::vector<std::pair<AddressBox const *,std::set<c_array>>>;
using StateOutputs=QHash<c_array,InBox>;
using InputMap=std::map<c_array,std::vector<std::pair<AddressBox const *,c_array>>>;

class QWALLET_EXPORT Wallet: public QObject
{
    Q_OBJECT
#if defined(USE_QML)
    Q_PROPERTY(Qml64*  amount READ amountJson CONSTANT)
    Q_PROPERTY(std::vector<AddressBox const *>  addresses READ getAddresses NOTIFY addressesChanged)
    QML_ELEMENT
    QML_SINGLETON
#endif
    Wallet(QObject *parent = nullptr);

public:
    static Wallet* instance();

    quint64 amount(void)const;
#if defined(USE_QML)
    static Wallet *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);
    Qml64* amountJson()const;
#endif
    const std::map<c_array,AddressBox const *> &  addresses()const;
    size_t nRootAddresses()const;
    std::vector<std::pair<c_array, std::shared_ptr<Output> > > getNFTs()const;

    InBox getInput(c_array id)const;
    std::vector<AddressBox const *> getAddresses();
    quint64 consume(InputSet& inputSet, StateOutputs &stateOutputs,
                    const quint64& amountNeedIt=0,
                    const std::set<Output::types>& onlyType={Output::All_typ},
                    const std::set<c_array> &outids={});
    std::pair<std::shared_ptr<const Payload>,std::set<QString>> createTransaction
        (const InputSet &inputSet, Node_info* info, const pvector<const Output> &outputs);
    void checkOutputs(std::vector<Node_output>  outs, AddressBox *addressBundle);
    void setAddressRange(quint32 range);
    void setAccountIndex(quint32 account);
signals:
    void addressesChanged(c_array);
    void inputAdded(c_array);
    void inputRemoved(c_array);
    void amountChanged();
    void synced();
private:
    void sync(QString hrp);
    void reset(void);
    void addAddress(AddressBox*addressBundle);

    pvector<const Unlock> createUnlocks(const InputSet& inputSet, const c_array &essenceHash)const;
    quint64 consumeInputs(const c_array outId, InputSet &inputSet, StateOutputs &stateOutputs);
    quint64 consumeInbox(const c_array outId, const InBox & inBox, StateOutputs &stateOutputs)const;
    bool getInput(const QString& input,InputSet& inputSet);
    void checkAddress(AddressBox *  addressBundle);
    quint64 m_amount;
#if defined(USE_QML)
    Qml64* m_amountJson;
#endif
    quint32 m_accountIndex, m_addressRange;
    InputMap m_outputs;
    std::set<QString> usedOutIds;
    std::map<c_array,AddressBox const *> m_addresses;
    std::vector<AddressBox *> m_rootAddresses;
    static Wallet * m_instance;

};

}
