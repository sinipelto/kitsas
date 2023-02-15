#ifndef TUONTI_PDFTIEDOSTO_H
#define TUONTI_PDFTIEDOSTO_H



#include "pdfsivu.h"
#include "tuontiapuinfo.h"

#include <QRegularExpression>

class QPdfDocument;

namespace Tuonti {

class PdfTiedosto
{
public:
    PdfTiedosto(QPdfDocument* doc);
    ~PdfTiedosto();

    const QVariantMap tuo(const TuontiApuInfo& info);

    QPdfDocument* pdfDoc() { return doc_; }
    PdfSivu* ekasivu();

    int sivumaara() const;
    PdfSivu* sivu(int sivu);

    QString kokoTeksti() const;

    QList<PdfPala*> etsiPalat(const QStringList& tekstit);

protected:
    QPdfDocument* doc_;
    QList<PdfSivu*> sivut_;

    static QRegularExpression ibanRe__;
};

} // namespace Tuonti

#endif // TUONTI_PDFTIEDOSTO_H
