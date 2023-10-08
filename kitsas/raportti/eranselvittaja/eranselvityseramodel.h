#ifndef ERANSELVITYSERAMODEL_H
#define ERANSELVITYSERAMODEL_H

#include <QAbstractTableModel>
#include <QDate>

#include "model/eramap.h"
#include "db/tili.h"

class EranSelvitysEraModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit EranSelvitysEraModel(QObject *parent = nullptr);

    enum { PVM, KUMPPANI, SELITE, SALDO};

    // Header:
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // Basic functionality:
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    void load(const int tili, const QDate& date);
    void refresh();

    void naytaNollatut(bool kaikki);

protected:
    class SelvitysEra {
    public:
        SelvitysEra();
        SelvitysEra(const QVariantMap& map);

        int id() const { return id_; }
        QDate pvm() const { return pvm_; }
        Euro saldo() const { return saldo_; }
        QString selite() const { return selite_; }
        QString nimi() const { return nimi_; }

        void setSelite(const QString& selite);

    protected:
        int id_;
        QDate pvm_;
        Euro saldo_;
        QString selite_;
        QString nimi_;
    };

private:
    void eratSaapuu(QVariant* data);

    QList<SelvitysEra> erat_;
    Tili tili_;
    QDate saldopvm_;
    bool nollatut_ = false;
};

#endif // ERANSELVITYSERAMODEL_H
