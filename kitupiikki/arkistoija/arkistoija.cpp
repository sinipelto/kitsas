/*
   Copyright (C) 2019 Arto Hyvättinen

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program. If not, see <http://www.gnu.org/licenses/>.
*/
#include "arkistoija.h"

#include "db/kirjanpito.h"

#include "raportti/raportoija.h"
#include "raportti/paakirja.h"
#include "raportti/paivakirja.h"
#include "raportti/taseerittelija.h"

#include <QFile>
#include <QTextStream>
#include <QFileInfo>

#include <QDebug>
#include <QApplication>

Arkistoija::Arkistoija(const Tilikausi &tilikausi, QObject *parent)
    : QObject(parent), tilikausi_(tilikausi)
{

}

void Arkistoija::arkistoi()
{
    luoHakemistot();
    arkistoiRaportit();
    arkistoiTositteet();
}

void Arkistoija::luoHakemistot()
{
    QDir hakemisto;

    hakemisto.mkpath( kp()->arkistopolku() );
    hakemisto_ = QDir( kp()->arkistopolku() );

    QString arkistonimi = tilikausi_.arkistoHakemistoNimi();

    if( hakemisto_.exists( arkistonimi ) )
    {
        // Jos hakemisto on jo olemassa, poistetaan se
        hakemisto_.cd( arkistonimi);
        hakemisto_.removeRecursively();
        hakemisto_.cdUp();
    }


    hakemisto_.mkdir( arkistonimi );
    hakemisto_.cd( arkistonimi );

    hakemisto_.mkdir("tositteet");
    hakemisto_.mkdir("liitteet");

    if( !kp()->logo().isNull() )
    {
        kp()->logo().save(hakemisto_.absoluteFilePath("logo.png"),"PNG");
        logo_ = true;
    }


    // Kopioidaan vakitiedostot
    QFile::copy( ":/arkisto/arkisto.css", hakemisto_.absoluteFilePath("arkisto.css"));
    QFile::copy( ":/arkisto/jquery.js", hakemisto_.absoluteFilePath("jquery.js"));
    QFile::copy( ":/arkisto/ohje.html", hakemisto_.absoluteFilePath("ohje.html"));
    QFile::copy( ":/pic/aboutpossu.png", hakemisto_.absoluteFilePath("kitupiikki.png"));

}

void Arkistoija::arkistoiTositteet()
{
    // Hakee tositeluettelon
    KpKysely* kysely = kpk("/tositteet");
    kysely->lisaaAttribuutti("jarjestys","tosite");
    kysely->lisaaAttribuutti("alkupvm", tilikausi_.alkaa());
    kysely->lisaaAttribuutti("loppupvm", tilikausi_.paattyy());
    connect( kysely, &KpKysely::vastaus, this, &Arkistoija::tositeLuetteloSaapuu );
    kysely->kysy();
}

void Arkistoija::arkistoiRaportit()
{
    raporttilaskuri_ = 5;
    Paivakirja* paivakirja = new Paivakirja(this);
    connect( paivakirja, &Paivakirja::valmis,
             [this] (RaportinKirjoittaja rk) { this->arkistoiRaportti(rk,"paivakirja.html"); } );
    paivakirja->kirjoita( tilikausi_.alkaa(), tilikausi_.paattyy() );

    Paakirja* paakirja = new Paakirja(this);
    connect( paakirja, &Paakirja::valmis,
             [this] (RaportinKirjoittaja rk) { this->arkistoiRaportti(rk,"paakirja.html"); } );
    paakirja->kirjoita( tilikausi_.alkaa(), tilikausi_.paattyy() );

    TaseErittelija* erittelija = new TaseErittelija(this);
    connect( erittelija, &TaseErittelija::valmis,
             [this] (RaportinKirjoittaja rk) { this->arkistoiRaportti(rk,"taseerittely.html"); } );
    erittelija->kirjoita(tilikausi_.alkaa(), tilikausi_.paattyy());

    Raportoija* tase = new Raportoija("tase/PMA","fi",this);
    connect( tase, &Raportoija::valmis,
             [this] (RaportinKirjoittaja rk) { this->arkistoiRaportti(rk,"tase.html"); } );
    tase->lisaaTasepaiva(tilikausi_.paattyy());
    tase->kirjoita(true, -1);

    Raportoija* tulos = new Raportoija("tulos/yhdistys","fi",this);
    connect( tulos, &Raportoija::valmis,
             [this] (RaportinKirjoittaja rk) { this->arkistoiRaportti(rk,"tuloslaskelma.html"); } );
    tulos->lisaaKausi(tilikausi_.alkaa(), tilikausi_.paattyy());
    tulos->kirjoita(true, -1);

}

void Arkistoija::arkistoiByteArray(const QString &tiedostonnimi, const QByteArray &array)
{
    QFile tiedosto( hakemisto_.absoluteFilePath(tiedostonnimi));
    tiedosto.open( QIODevice::WriteOnly);
    tiedosto.write( array );
    tiedosto.close();
}

void Arkistoija::tositeLuetteloSaapuu(QVariant *data)
{
    // Lisätään tositteet luetteloon
    QVariantList lista( data->toList() );
    for( auto tosite : lista ) {
        QVariantMap map = tosite.toMap();
        tositeJono_.append( map );
    }
    arkistoiSeuraavaTosite();
}

void Arkistoija::arkistoiSeuraavaTosite()
{
    // Hae tosite arkistoitavaksi
    int indeksi = arkistoitavaTosite_;
    arkistoitavaTosite_++;

    KpKysely* kysely = kpk(QString("/tositteet/%1").arg( tositeJono_.value(indeksi).id() ));
    connect( kysely, &KpKysely::vastaus,
             [this, indeksi] (QVariant* data) { this->arkistoiTosite(data, indeksi);} );

    kysely->kysy();
}

void Arkistoija::arkistoiTosite(QVariant *data, int indeksi)
{
    // Lisätään ensin liitteet luetteloille
    QVariantMap map = data->toMap();
    int liitenro = 1;
    for( auto liite : map.value("liitteet").toList()) {
        QVariantMap liitemap = liite.toMap();
        QString tnimi = liitemap.value("nimi").toString();
        int liiteid = liitemap.value("id").toInt();
        QString liitenimi = QString("%1-%2-%3_%4.%5")
                .arg( kp()->tilikaudet()->tilikausiPaivalle( map.value("pvm").toDate() ).pitkakausitunnus() )
                .arg( map.value("sarja").toString() )
                .arg( map.value("tunniste").toInt(), 8, 10, QChar('0'))
                .arg( liitenro, 2, 10, QChar('0'))
                .arg( tnimi.mid(tnimi.indexOf('.')+1)  );
        liiteNimet_.insert( liiteid, liitenimi );
        liiteJono_.enqueue( liiteid );
    }

    qDebug() << "Arkistoon " << indeksi << " id " << map.value("id").toInt() << " < " << tositeJono_.count();

    QString nimi = "tositteet/" + tositeJono_.value(indeksi).tiedostonnimi();

    arkistoiByteArray( nimi, tosite(map, indeksi) );

    if( arkistoitavaTosite_ < tositeJono_.count())
        arkistoiSeuraavaTosite();
    else if( !liiteJono_.isEmpty() )
        arkistoiSeuraavaLiite();
    else if( !raporttilaskuri_ )
        viimeistele();

}

void Arkistoija::arkistoiSeuraavaLiite()
{
    int liiteid = liiteJono_.dequeue();
    QString tiedosto = liiteNimet_.value(liiteid);
    KpKysely* kysely = kpk(QString("/liitteet/%1").arg(liiteid));
    connect( kysely, &KpKysely::vastaus, [this, tiedosto] (QVariant* data) { this->arkistoiLiite(data, tiedosto);  });
    kysely->kysy();
}

void Arkistoija::arkistoiLiite(QVariant *data, const QString tiedosto)
{
    arkistoiByteArray("liitteet/" + tiedosto, data->toByteArray());
    if( !liiteJono_.isEmpty())
        arkistoiSeuraavaLiite();
    else if( !raporttilaskuri_ )
        viimeistele();
}

void Arkistoija::arkistoiRaportti(RaportinKirjoittaja rk, const QString &tiedosto)
{
    QString txt = rk.html();
    txt.insert( txt.indexOf("</head>"), "<link rel='stylesheet' type='text/css' href='arkisto.css'>");
    txt.insert( txt.indexOf("<body>") + 6, navipalkki( ));

    arkistoiByteArray( tiedosto, txt.toUtf8() );
    raporttilaskuri_--;
    if( !raporttilaskuri_ && liiteJono_.isEmpty() && arkistoitavaTosite_ >= tositeJono_.count())
        viimeistele();
}

void Arkistoija::viimeistele()
{
    QFile tiedosto( hakemisto_.absoluteFilePath("index.html"));
    tiedosto.open( QIODevice::WriteOnly);
    QTextStream out( &tiedosto );
    out.setCodec("UTF-8");

    out << "<html><meta charset=\"UTF-8\"><head><title>";
    out << kp()->asetus("Nimi") + " arkisto";
    out << "</title><link rel='stylesheet' type='text/css' href='arkisto.css'></head><body>";

    out << navipalkki();

    if(logo_)
        out << "<img src=logo.png class=logo>";

    out << "<h1 class=etusivu>" << kp()->asetus("Nimi") << "</h1>";
    out << "<h2 class=etusivu>Kirjanpitoarkisto<br>" ;
    out << tilikausi_.kausivaliTekstina();
    out << "</h2>";

    // Jos tilit on päätetty (tilikausi lukittu), tulee linkki myös tilinpäätökseen (pdf)
    if( tilikausi_.paattyy() <= kp()->tilitpaatetty() )
        out << "<h3>" << tr("Tilinpäätös") << "</h3>"
            << "<ul><li><a href=tilinpaatos.pdf>" << tr("Tilinpäätös") << "</a></li>"
            << "<li><a href=taseerittely.html>" << tr("Tase-erittely") << "</a></li></ul>";


    out << "<h3>Kirjanpito</h3>";
    out << "<ul><li><a href=paakirja.html>" << tr("Pääkirja") << "</a></li>";
    out << "<li><a href=paivakirja.html>" << tr("Päiväkirja") << "</a></li>";
//    out << "<li><a href=tositeluettelo.html>Tositeluettelo</a></li>";
//    out << "<li><a href=tositepaivakirja.html>" << tr("Tositepäiväkirja") << "</a></li>";
//    out << "<li><a href=tililuettelo.html>Tililuettelo</a></li>";
    out << "</ul><h3>Raportit</h3><ul>";



    out << tr("<p class=info>Tämä kirjanpidon sähköinen arkisto on luotu %1 <a href=https://kitupiikki.info>Kitupiikki-ohjelman</a> versiolla %2 <br>")
           .arg(QDate::currentDate().toString(Qt::SystemLocaleDate))
           .arg(qApp->applicationVersion());

    if( tilikausi_.paattyy() > kp()->tilitpaatetty() )
        out << "Kirjanpito on viel&auml; keskener&auml;inen.";

    out << "</body></html>";

}

QByteArray Arkistoija::tosite(const QVariantMap& tosite, int indeksi)
{
    QByteArray ba;
    QTextStream out (&ba);
    out.setCodec("utf-8");

    out << "<html><meta charset=\"UTF-8\"><head><title>" << tosite.value("otsikko").toString() << "</title>";
    out << "<link rel='stylesheet' type='text/css' href='../arkisto.css'></head><body>";

    out << navipalkki(indeksi);

    const QVariantList& liitteet = tosite.value("liitteet").toList();

    // LIITTEET

    if( liitteet.count() )
    {
        // Liitteen laatikko, johon nykyinen liite ladataan
        out << "<iframe width='100%' height='50%' class='liite' id='liite' src='../liitteet/";
        out << liiteNimet_.value( liitteet.value(0).toMap().value("id").toInt() );
        out <<  "'></iframe>";

        out << "<table class='liiteluettelo'>";

        // luettelo
        for( auto liite : liitteet) {
            const QVariantMap& liitemap = liite.toMap();
            const QString liitetiedosto = liiteNimet_.value( liitemap.value("id").toInt() );
            out << "<tr><td onclick=\"$('#liite').attr('src','../liitteet/"
                 << liitetiedosto
                 << "');\">" << liitemap.value("nimi").toString()
                 << "</td><td><a href='../liitteet/" << liitetiedosto
                 << "' class=avaaliite>Avaa</a></td></tr>\n";
        }
        out << "</table>";
    }

    // Seuraavaksi otsikot
    out << "<table class=tositeotsikot><tr>";
    out << "<td class=paiva>" << tosite.value("pvm").toDate().toString("dd.MM.yyyy") << "</td>";
    out << "<td class=tositeotsikko>" << tosite.value("otsikko").toString() << "</td>";

    out << QString("<td class=tositetunnus>%1")
           .arg(kp()->tositeTunnus( tosite.value("tunniste").toInt(),
                                    tosite.value("pvm").toDate(),
                                    tosite.value("sarja").toString()));
    out << "</tr></table>";


    // Viennit
    out << "<table class=viennit>";
    out <<  "<tr><th>Pvm</th><th>Tili</th><th>Kohdennus</th><th>Selite</th><th>Debet</th><th>Kredit</th></tr>";

    for( auto vienti : tosite.value("viennit").toList())    {
        QVariantMap vientiMap = vienti.toMap();
        Tili* tili = kp()->tilit()->tili( vientiMap.value("tili").toInt() );
        if( !tili)
            continue;

        out << "<tr><td class=pvm>" << vientiMap.value("pvm").toDate().toString("dd.MM.yyyy") ;
        out << "</td><td class=tili><a href='../paakirja.html#" << tili->numero() << "'>"
            << tili->nimiNumero() << "</a>";

        out << "</td><td class=kohdennus>";
        if( vientiMap.value("kohdennus").toInt())
            out << kp()->kohdennukset()->kohdennus( vientiMap.value("kohdennus").toInt() ).nimi();

        // TODO: Merkkaukset ja tase-erät

        out << "</td><td class=selite>" << vientiMap.value("selite").toString();
        out << "</td><td class=euro>" << ((qAbs(vientiMap.value("debet").toDouble()) > 1e-5) ?  QString("%L1 €").arg(vientiMap.value("debet").toDouble(),0,'f',2) : "");
        out << "</td><td class=euro>" << ((qAbs(vientiMap.value("kredit").toDouble()) > 1e-5) ?  QString("%L1 €").arg(vientiMap.value("kredit").toDouble(),0,'f',2) : "");
        out << "</td></tr>\n";
    }
    out << "</table>";

    // Kommentit
    if( !tosite.value("info").toString().isEmpty())
    {
        out << "<p class=kommentti>";
        out << tosite.value("info").toString().toHtmlEscaped().replace("\n","<br>");
        out << "</p>";
    }

    out << "<p class=info>Kirjanpito arkistoitu " << QDate::currentDate().toString(Qt::SystemLocaleDate);
    out << "<script src='../jquery.js'></script>";
    out << "</body></html>";

    out.flush();
    return ba;
}

QString Arkistoija::tiedostonnimi(const QDate &pvm, const QString &sarja, int tunniste)
{
    return QString("%1-%2-%3.html")
            .arg( kp()->tilikaudet()->tilikausiPaivalle(pvm).pitkakausitunnus() )
            .arg( sarja )
            .arg( tunniste, 8, 10, QChar('0'));
}

QString Arkistoija::navipalkki(int indeksi) const
{
    QString navi = "<nav><ul><li class=kotinappi><a href=";
    if( indeksi > -1)
        navi.append("../");     // Tositteista palataan päähakemistoon

    navi.append("index.html>");
    if( logo_ )
        navi.append("<img src=logo.png>");
    navi.append( kp()->asetus("Nimi") + " ");

    if( kp()->onkoHarjoitus())
        navi.append("<span class=treeni>HARJOITUS </span>");

    navi.append(tilikausi_.kausivaliTekstina());
    navi.append("</a></li>");


    if( indeksi > -1 && indeksi + 1 < tositeJono_.count() )
        navi.append( tr("<li class=nappi><a href='%1'>Seuraava &rarr;</a></li>")
                     .arg( tositeJono_.value(indeksi+1).tiedostonnimi() ) );
    else
        navi.append( "<li class=nappi> </li>");

    if( indeksi > 0 )
        navi.append( tr("<li class=nappi><a href='%1'>&larr; Edellinen</a></li>")
                     .arg(tositeJono_.value(indeksi-1).tiedostonnimi()) );
    else
        navi.append("<li class=nappi> </li>");


    navi.append("</ul></nav></div>");

    return navi;
}


Arkistoija::JonoTosite::JonoTosite() :
    tunniste_(0), id_(0)
{

}

Arkistoija::JonoTosite::JonoTosite(const QString &sarja, int tunniste, int id, const QDate &pvm) :
    sarja_(sarja), tunniste_(tunniste), id_(id), pvm_(pvm)
{

}

Arkistoija::JonoTosite::JonoTosite(const QVariantMap &map)
    : sarja_(map.value("sarja").toString()),
      tunniste_( map.value("tunniste").toInt()),
      id_( map.value("id").toInt()),
      pvm_( map.value("pvm").toDate())
{

}

QString Arkistoija::JonoTosite::tiedostonnimi()
{
    return Arkistoija::tiedostonnimi(pvm(), sarja(), tunniste());
}
