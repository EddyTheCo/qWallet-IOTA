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


using InputSet = std::vector<std::pair<AddressBox*,std::set<c_array>>>;
using StateOutputs=QHash<QString,InBox>;
using InputMap=std::map<c_array,std::vector<std::pair<AddressBox*,c_array>>>;

class QWALLET_EXPORT Wallet: public QObject
{
    Q_OBJECT
#if defined(USE_QML)
    Q_PROPERTY(Qml64*  amount READ amountJson CONSTANT)
    Q_PROPERTY(std::vector<AddressBox*>  addresses READ getAddresses NOTIFY addressesChanged)
    QML_ELEMENT
    QML_SINGLETON
#endif
    Wallet(QObject *parent = nullptr);

public:
    static Wallet* instance();

    auto amount(void)const{return m_amount;};
#if defined(USE_QML)
    static Wallet *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
    {
        return instance();
    }
    Qml64* amountJson()const{return m_amountJson;}
#endif
    const auto & addresses()const{return m_addresses;}
    const auto & inputs()const{return m_outputs;}
    std::vector<AddressBox*> getAddresses();
    quint64 consume(InputSet& inputSet, StateOutputs &stateOutputs,
                    const quint64& amountNeedIt=0,
                    const std::set<Output::types>& onlyType={Output::All_typ},
                    const std::set<c_array> &outids={});
    std::pair<std::shared_ptr<const Payload>,std::set<QString>> createTransaction
        (const InputSet &inputSet, Node_info* info, const pvector<const Output> &outputs);
    void checkOutputs(std::vector<Node_output>  outs, AddressBox *addressBundle);
signals:
    void addressesChanged(c_array);
    void amountChanged();
    void ready();
    void synced();
private:
    void sync(void);
    void reset(void);
    void addAddress(AddressBox*addressBundle);
    pvector<const Unlock> createUnlocks(const InputSet& inputSet, const c_array &essenceHash)const;
    quint64 consumeInputs(const c_array &outId, InputSet &inputSet, StateOutputs &stateOutputs);
    quint64 consumeInbox(const QString & outId,const InBox & inBox, StateOutputs &stateOutputs)const;
    bool getInput(const QString& input,InputSet& inputSet);
    void checkAddress(AddressBox *  addressBundle);
    quint64 m_amount;
#if defined(USE_QML)
    Qml64* m_amountJson;
#endif
    quint32 accountIndex, addressRange;
    InputMap m_outputs; //use better a map for ordering the inputs to consume them
    std::set<QString> usedOutIds;
    std::map<c_array,AddressBox*> m_addresses;
    static Wallet * m_instance;


};

}
