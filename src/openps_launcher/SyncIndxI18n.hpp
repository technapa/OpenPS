// SPDX-License-Identifier: AGPL-3.0-or-later
// ============================================================================
// SyncIndxI18n.hpp - OpenSlicer user interface strings
//
// * kLangs: every string added by OpenSlicer, in the 21 languages shipped with
//   PrusaSlicer (be, ca, cs, de, en, es, fi, fr, hu, it, ja, ko, nl, pl, pt_BR,
//   ru, sl, tr, uk, zh_CN, zh_TW). Wording shared with Prusa Connect follows the
//   official Prusa Connect translations.
// * AppCatalog: read access to PrusaSlicer's own .mo catalog, for words that
//   PrusaSlicer already translates (view names, context menu entries).
//
// The language is the one selected in PrusaSlicer (translation_language in
// PrusaSlicer.ini), falling back to the Windows UI language, then English.
// Set OPENPS_SYNC_LANG=<code> to force a language while testing.
//
// Adding a string: append an id before S_COUNT, then append the text at the
// same position in EVERY language block (same {0} {1} placeholders).
// ============================================================================
#pragma once
#ifndef OPENPS_SYNC_INDX_I18N_HPP
#define OPENPS_SYNC_INDX_I18N_HPP

#include <windows.h>
#include <shlobj.h>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <string>

namespace openps::sync::i18n {

enum StrId : int {
    S_INSTALLED_IN = 0,   // {0} = printer name
    S_ACTIVE_IN,          // {0} = application name
    S_TOOL,
    S_FILAMENT,
    S_CHANGE,
    S_ASSIGN,
    S_UNLOAD,
    S_COLOR,
    S_SEND_TO,            // {0} = destination
    S_REFRESH,
    S_CANCEL,
    S_APPLY,
    S_NO_MATERIAL,
    S_UNKNOWN_MATERIAL,
    S_INACTIVE,
    S_NO_PRINTER,
    S_STATE_IDLE,
    S_STATE_READY,
    S_STATE_PRINTING,
    S_STATE_BUSY,
    S_STATE_OFFLINE,
    S_ST_READING,
    S_ST_NOT_LOGGED,
    S_ST_UNREACHABLE,
    S_ST_EXPIRED,
    S_ST_HTTP,            // {0} = HTTP code
    S_ST_NO_PRINTER,
    S_ST_UPDATED,         // {0} = printer, {1} = loaded, {2} = total
    S_ST_SENT_SLICER,     // {0} = count, {1} = application name
    S_ST_SENDING,         // {0} = destination
    S_ST_SENT_MACHINE,    // {0} = printer
    S_ST_LAN,
    S_ST_BUSY,            // {0} = printer state
    S_ST_SEND_FAILED,     // {0} = HTTP code
    S_ST_NOTHING,
    S_ST_ASSIGNED,        // {0} = material
    S_ST_ASSIGN_FAILED,   // {0} = reason
    S_ST_UNLOAD_SENT,     // {0} = tool
    S_ST_UNLOAD_FAILED,   // {0} = reason
    S_DLG_ASSIGN_TO,      // {0} = tool
    S_DLG_SEARCH,
    S_DLG_LOADING,
    S_DLG_NONE,
    S_DLG_FAILED,
    S_DLG_CUSTOM,
    S_CONFIRM_UNLOAD,     // {0} = tool
    // Launcher (ViewCube, Set origin, window title, startup errors)
    S_SET_ORIGIN,
    S_ORIGIN_OBJECT,
    S_ORIGIN_BED,
    S_ORIGIN_PILL,  // short label of the ViewCube button
    S_TITLE_BASED_ON,  // {0} = PrusaSlicer version
    S_ERR_TITLE,
    S_ERR_LOAD_DLL,  // {0} = path, {1} = Windows error code
    S_ERR_ENTRY,  // {0} = path
    S_DLL_MISMATCH,  // {0} = PrusaSlicer version
    S_COUNT
};

struct LangTable {
    const wchar_t* code;
    const wchar_t* s[S_COUNT];
};

inline constexpr LangTable kLangs[] = {
    { L"en", {
        L"Filaments installed in {0}", L"Active filaments in {0}", L"Tool", L"Filament",
        L"Change", L"Assign", L"Unload", L"Color", L"Send to {0}", L"Refresh", L"Cancel", L"Apply",
        L"No material assigned", L"Unknown material", L"Inactive", L"No printer",
        L"Idle", L"Ready", L"Printing", L"Busy", L"Offline",
        L"Reading Prusa Connect…",
        L"Not signed in. Log in from the Prusa Connect tab.",
        L"Prusa Connect is unreachable.",
        L"Session expired. Log in again from the Prusa Connect tab.",
        L"Prusa Connect error (HTTP {0}).",
        L"No printer found on this account.",
        L"{0}: {1} of {2} tools loaded.",
        L"{0} filaments sent to {1}.",
        L"Sending to {0}…",
        L"Filaments sent to {0}.",
        L"Printer unreachable on the local network.",
        L"Not sent: the printer is busy ({0}).",
        L"Sending failed (HTTP {0}).",
        L"No filament to send.",
        L"Material assigned: {0}",
        L"Assignment failed: {0}",
        L"Unload requested for {0}.",
        L"Unload failed: {0}",
        L"Assign a material to {0}",
        L"Search materials, brands or types…",
        L"Loading…", L"No material found.", L"Could not load materials.", L"Custom material",
        L"Unload the filament from {0}? The printer will heat the nozzle.",
        L"Set origin",
        L"Center of active object",
        L"Center of build plate",
        L"ORIGIN",
        L"based on PrusaSlicer {0}",
        L"OpenSlicer error",
        L"Unable to load PrusaSlicer.dll:\n{0}\nError code: {1}",
        L"The slic3r_main entry point was not found in {0}.",
        L"This PrusaSlicer.dll is not the PrusaSlicer {0} build OpenSlicer was made for.\nThe ViewCube and Set origin tools are disabled. Slicing and the Sync tab still work."
    } },
    { L"fr", {
        L"Filaments installés dans {0}", L"Filaments actifs dans {0}", L"Outil", L"Filament",
        L"Modifier", L"Attribuer", L"Décharger", L"Couleur", L"Envoyer vers {0}", L"Actualiser", L"Annuler", L"Appliquer",
        L"Aucun matériau attribué", L"Matériau inconnu", L"Inactif", L"Aucune imprimante",
        L"Inactive", L"Prête", L"Impression", L"Occupée", L"Hors ligne",
        L"Lecture de Prusa Connect…",
        L"Non connecté. Connectez-vous depuis l'onglet Prusa Connect.",
        L"Prusa Connect est injoignable.",
        L"Session expirée. Reconnectez-vous depuis l'onglet Prusa Connect.",
        L"Erreur Prusa Connect (HTTP {0}).",
        L"Aucune imprimante trouvée sur ce compte.",
        L"{0} : {1} outils chargés sur {2}.",
        L"{0} filaments envoyés vers {1}.",
        L"Envoi vers {0}…",
        L"Filaments envoyés vers {0}.",
        L"Imprimante injoignable sur le réseau local.",
        L"Non envoyé : l'imprimante est occupée ({0}).",
        L"Échec de l'envoi (HTTP {0}).",
        L"Aucun filament à envoyer.",
        L"Matériau attribué : {0}",
        L"Échec de l'attribution : {0}",
        L"Déchargement demandé pour {0}.",
        L"Échec du déchargement : {0}",
        L"Attribuer un matériau à {0}",
        L"Rechercher des matériaux, des marques ou des types…",
        L"Chargement…", L"Aucun matériau trouvé.", L"Impossible de charger les matériaux.", L"Matériau personnalisé",
        L"Décharger le filament de {0} ? L'imprimante va chauffer la buse.",
        L"Définir l'origine",
        L"Au centre de l'objet actif",
        L"Au centre du plateau",
        L"ORIGINE",
        L"basé sur PrusaSlicer {0}",
        L"Erreur OpenSlicer",
        L"Impossible de charger PrusaSlicer.dll :\n{0}\nCode d'erreur : {1}",
        L"Le point d'entrée slic3r_main est introuvable dans {0}.",
        L"Ce PrusaSlicer.dll n'est pas la version PrusaSlicer {0} prévue pour OpenSlicer.\nLe ViewCube et « Définir l'origine » sont désactivés. Le tranchage et l'onglet Sync fonctionnent normalement."
    } },
    { L"de", {
        L"Installierte Filamente in {0}", L"Aktive Filamente in {0}", L"Werkzeug", L"Filament",
        L"Ändern", L"Zuweisen", L"Entladen", L"Farbe", L"An {0} senden", L"Aktualisieren", L"Abbrechen", L"Anwenden",
        L"Kein Material zugewiesen", L"Unbekanntes Material", L"Inaktiv", L"Kein Drucker",
        L"Leerlauf", L"Bereit", L"Druckt", L"Beschäftigt", L"Offline",
        L"Prusa Connect wird gelesen…",
        L"Nicht angemeldet. Melden Sie sich im Reiter Prusa Connect an.",
        L"Prusa Connect ist nicht erreichbar.",
        L"Sitzung abgelaufen. Melden Sie sich im Reiter Prusa Connect erneut an.",
        L"Prusa Connect-Fehler (HTTP {0}).",
        L"Kein Drucker in diesem Konto gefunden.",
        L"{0}: {1} von {2} Werkzeugen geladen.",
        L"{0} Filamente an {1} gesendet.",
        L"Senden an {0}…",
        L"Filamente an {0} gesendet.",
        L"Drucker im lokalen Netzwerk nicht erreichbar.",
        L"Nicht gesendet: Der Drucker ist beschäftigt ({0}).",
        L"Senden fehlgeschlagen (HTTP {0}).",
        L"Kein Filament zu senden.",
        L"Material zugewiesen: {0}",
        L"Zuweisung fehlgeschlagen: {0}",
        L"Entladen für {0} angefordert.",
        L"Entladen fehlgeschlagen: {0}",
        L"Ein Material zuweisen zu {0}",
        L"Marken, Material oder Typ suchen…",
        L"Wird geladen…", L"Kein Material gefunden.", L"Materialien konnten nicht geladen werden.", L"Eigenes Material",
        L"Filament aus {0} entladen? Der Drucker heizt die Düse auf.",
        L"Ursprung festlegen",
        L"Mitte des aktiven Objekts",
        L"Mitte der Druckplatte",
        L"URSPRUNG",
        L"basierend auf PrusaSlicer {0}",
        L"OpenSlicer-Fehler",
        L"PrusaSlicer.dll konnte nicht geladen werden:\n{0}\nFehlercode: {1}",
        L"Der Einstiegspunkt slic3r_main wurde in {0} nicht gefunden.",
        L"Diese PrusaSlicer.dll entspricht nicht dem PrusaSlicer-{0}-Build, für den OpenSlicer erstellt wurde.\nViewCube und „Ursprung festlegen“ sind deaktiviert. Slicing und der Sync-Reiter funktionieren weiterhin."
    } },
    { L"es", {
        L"Filamentos instalados en {0}", L"Filamentos activos en {0}", L"Cabezal", L"Filamento",
        L"Cambiar", L"Asignar", L"Descargar", L"Color", L"Enviar a {0}", L"Actualizar", L"Cancelar", L"Aplicar",
        L"Ningún material asignado", L"Material desconocido", L"Inactivo", L"Ninguna impresora",
        L"En reposo", L"Lista", L"Imprimiendo", L"Ocupada", L"Fuera de línea",
        L"Leyendo Prusa Connect…",
        L"Sesión no iniciada. Inicia sesión desde la pestaña Prusa Connect.",
        L"No se puede acceder a Prusa Connect.",
        L"Sesión caducada. Vuelve a iniciar sesión desde la pestaña Prusa Connect.",
        L"Error de Prusa Connect (HTTP {0}).",
        L"No se encontró ninguna impresora en esta cuenta.",
        L"{0}: {1} de {2} cabezales cargados.",
        L"{0} filamentos enviados a {1}.",
        L"Enviando a {0}…",
        L"Filamentos enviados a {0}.",
        L"Impresora inaccesible en la red local.",
        L"No enviado: la impresora está ocupada ({0}).",
        L"Error de envío (HTTP {0}).",
        L"Ningún filamento que enviar.",
        L"Material asignado: {0}",
        L"Asignación fallida: {0}",
        L"Descarga solicitada para {0}.",
        L"Descarga fallida: {0}",
        L"Asignar un material a {0}",
        L"Buscar materiales, marcas o tipos…",
        L"Cargando…", L"No se encontró ningún material.", L"No se pudo cargar los materiales.", L"Material personalizado",
        L"¿Descargar el filamento de {0}? La impresora calentará la boquilla.",
        L"Definir origen",
        L"Centro del objeto activo",
        L"Centro de la base de impresión",
        L"ORIGEN",
        L"basado en PrusaSlicer {0}",
        L"Error de OpenSlicer",
        L"No se puede cargar PrusaSlicer.dll:\n{0}\nCódigo de error: {1}",
        L"No se encontró el punto de entrada slic3r_main en {0}.",
        L"Este PrusaSlicer.dll no es la compilación de PrusaSlicer {0} para la que se hizo OpenSlicer.\nEl ViewCube y «Definir origen» están desactivados. El laminado y la pestaña Sync siguen funcionando."
    } },
    { L"it", {
        L"Filamenti installati in {0}", L"Filamenti attivi in {0}", L"Strumento", L"Filamento",
        L"Cambia", L"Assegna", L"Scarica", L"Colore", L"Invia a {0}", L"Aggiorna", L"Annulla", L"Applica",
        L"Nessun materiale assegnato", L"Materiale sconosciuto", L"Inattivo", L"Nessuna stampante",
        L"Inattiva", L"Pronta", L"In stampa", L"Occupata", L"Offline",
        L"Lettura di Prusa Connect…",
        L"Accesso non effettuato. Accedi dalla scheda Prusa Connect.",
        L"Prusa Connect non è raggiungibile.",
        L"Sessione scaduta. Accedi di nuovo dalla scheda Prusa Connect.",
        L"Errore di Prusa Connect (HTTP {0}).",
        L"Nessuna stampante trovata in questo account.",
        L"{0}: {1} strumenti caricati su {2}.",
        L"{0} filamenti inviati a {1}.",
        L"Invio a {0}…",
        L"Filamenti inviati a {0}.",
        L"Stampante non raggiungibile sulla rete locale.",
        L"Non inviato: la stampante è occupata ({0}).",
        L"Invio non riuscito (HTTP {0}).",
        L"Nessun filamento da inviare.",
        L"Materiale assegnato: {0}",
        L"Assegnazione fallita: {0}",
        L"Scaricamento richiesto per {0}.",
        L"Scaricamento non riuscito: {0}",
        L"Assegna un materiale a {0}",
        L"Cerca materiali, marche o tipi…",
        L"Caricamento…", L"Nessun materiale trovato.", L"Impossibile caricare i materiali.", L"Materiale personalizzato",
        L"Scaricare il filamento da {0}? La stampante riscalderà l'ugello.",
        L"Imposta origine",
        L"Centro dell'oggetto attivo",
        L"Centro del piano",
        L"ORIGINE",
        L"basato su PrusaSlicer {0}",
        L"Errore di OpenSlicer",
        L"Impossibile caricare PrusaSlicer.dll:\n{0}\nCodice di errore: {1}",
        L"Punto di ingresso slic3r_main non trovato in {0}.",
        L"Questa PrusaSlicer.dll non è la build di PrusaSlicer {0} per cui è stato creato OpenSlicer.\nViewCube e «Imposta origine» sono disattivati. Lo slicing e la scheda Sync funzionano comunque."
    } },
    { L"pl", {
        L"Filamenty zainstalowane w {0}", L"Aktywne filamenty w {0}", L"Narzędzie", L"Filament",
        L"Zmień", L"Przypisz", L"Rozładuj", L"Kolor", L"Wyślij do {0}", L"Odśwież", L"Anuluj", L"Zastosuj",
        L"Nie przypisano materiału", L"Nieznany materiał", L"Nieaktywny", L"Brak drukarki",
        L"Bezczynna", L"Gotowa", L"Drukowanie", L"Zajęta", L"Offline",
        L"Odczyt z Prusa Connect…",
        L"Nie zalogowano. Zaloguj się w zakładce Prusa Connect.",
        L"Prusa Connect jest niedostępne.",
        L"Sesja wygasła. Zaloguj się ponownie w zakładce Prusa Connect.",
        L"Błąd Prusa Connect (HTTP {0}).",
        L"Nie znaleziono drukarki na tym koncie.",
        L"{0}: załadowane narzędzia: {1} z {2}.",
        L"Wysłano filamenty ({0}) do {1}.",
        L"Wysyłanie do {0}…",
        L"Wysłano filamenty do {0}.",
        L"Drukarka niedostępna w sieci lokalnej.",
        L"Nie wysłano: drukarka jest zajęta ({0}).",
        L"Wysyłanie nie powiodło się (HTTP {0}).",
        L"Brak filamentu do wysłania.",
        L"Przypisano materiał: {0}",
        L"Przypisanie nie powiodło się: {0}",
        L"Zażądano rozładowania: {0}.",
        L"Rozładowanie nie powiodło się: {0}",
        L"Przypisz materiał do: {0}",
        L"Szukaj materiałów, marek lub typów…",
        L"Wczytywanie…", L"Nie znaleziono materiału.", L"Nie udało się wczytać materiałów.", L"Materiał niestandardowy",
        L"Rozładować filament z: {0}? Drukarka nagrzeje dyszę.",
        L"Ustaw punkt odniesienia",
        L"Środek aktywnego obiektu",
        L"Środek stołu",
        L"ŚRODEK",
        L"na podstawie PrusaSlicer {0}",
        L"Błąd OpenSlicer",
        L"Nie można załadować PrusaSlicer.dll:\n{0}\nKod błędu: {1}",
        L"Nie znaleziono punktu wejścia slic3r_main w {0}.",
        L"Ten plik PrusaSlicer.dll nie jest wersją PrusaSlicer {0}, dla której przygotowano OpenSlicer.\nViewCube i „Ustaw punkt odniesienia” są wyłączone. Cięcie i karta Sync działają normalnie."
    } },
    { L"cs", {
        L"Filamenty nainstalované v {0}", L"Aktivní filamenty v {0}", L"Nástroj", L"Filament",
        L"Změnit", L"Přiřadit", L"Vysunout", L"Barva", L"Odeslat do {0}", L"Obnovit", L"Zrušit", L"Použít",
        L"Žádný přiřazený materiál", L"Neznámý materiál", L"Neaktivní", L"Žádná tiskárna",
        L"Nečinná", L"Připravena", L"Tiskne", L"Zaneprázdněna", L"Offline",
        L"Načítání z Prusa Connect…",
        L"Nepřihlášeno. Přihlaste se na kartě Prusa Connect.",
        L"Prusa Connect není dostupný.",
        L"Relace vypršela. Přihlaste se znovu na kartě Prusa Connect.",
        L"Chyba Prusa Connect (HTTP {0}).",
        L"Na tomto účtu nebyla nalezena žádná tiskárna.",
        L"{0}: zavedené nástroje: {1} z {2}.",
        L"Odeslané filamenty ({0}) do {1}.",
        L"Odesílání do {0}…",
        L"Filamenty odeslány do {0}.",
        L"Tiskárna není dostupná v místní síti.",
        L"Neodesláno: tiskárna je zaneprázdněna ({0}).",
        L"Odeslání se nezdařilo (HTTP {0}).",
        L"Žádný filament k odeslání.",
        L"Materiál přiřazen: {0}",
        L"Přiřazení se nezdařilo: {0}",
        L"Vysunutí vyžádáno: {0}.",
        L"Vysunutí se nezdařilo: {0}",
        L"Přiřadit materiál k: {0}",
        L"Hledat materiály, značky nebo typy…",
        L"Načítání…", L"Žádný materiál nenalezen.", L"Materiály se nepodařilo načíst.", L"Vlastní materiál",
        L"Vysunout filament z: {0}? Tiskárna nahřeje trysku.",
        L"Nastavit počátek",
        L"Střed aktivního objektu",
        L"Střed tiskové podložky",
        L"POČÁTEK",
        L"založeno na PrusaSlicer {0}",
        L"Chyba OpenSlicer",
        L"Nelze načíst PrusaSlicer.dll:\n{0}\nKód chyby: {1}",
        L"Vstupní bod slic3r_main nebyl v {0} nalezen.",
        L"Tento soubor PrusaSlicer.dll není sestavení PrusaSlicer {0}, pro které byl OpenSlicer vytvořen.\nViewCube a „Nastavit počátek“ jsou vypnuté. Slicování a karta Sync fungují dál."
    } },
    { L"nl", {
        L"Filamenten geïnstalleerd in {0}", L"Actieve filamenten in {0}", L"Tool", L"Filament",
        L"Wijzigen", L"Toewijzen", L"Ontladen", L"Kleur", L"Verzenden naar {0}", L"Vernieuwen", L"Annuleren", L"Toepassen",
        L"Geen materiaal toegewezen", L"Onbekend materiaal", L"Inactief", L"Geen printer",
        L"Inactief", L"Gereed", L"Bezig met printen", L"Bezet", L"Offline",
        L"Prusa Connect wordt gelezen…",
        L"Niet aangemeld. Meld je aan via het tabblad Prusa Connect.",
        L"Prusa Connect is niet bereikbaar.",
        L"Sessie verlopen. Meld je opnieuw aan via het tabblad Prusa Connect.",
        L"Prusa Connect-fout (HTTP {0}).",
        L"Geen printer gevonden in dit account.",
        L"{0}: {1} van {2} tools geladen.",
        L"{0} filamenten verzonden naar {1}.",
        L"Verzenden naar {0}…",
        L"Filamenten verzonden naar {0}.",
        L"Printer niet bereikbaar op het lokale netwerk.",
        L"Niet verzonden: de printer is bezet ({0}).",
        L"Verzenden mislukt (HTTP {0}).",
        L"Geen filament om te verzenden.",
        L"Materiaal toegewezen: {0}",
        L"Toewijzing mislukt: {0}",
        L"Ontladen aangevraagd voor {0}.",
        L"Ontladen mislukt: {0}",
        L"Een materiaal toewijzen aan {0}",
        L"Zoek materialen, merken of types…",
        L"Laden…", L"Geen materiaal gevonden.", L"Materialen konden niet worden geladen.", L"Aangepast materiaal",
        L"Filament uit {0} ontladen? De printer verwarmt de nozzle.",
        L"Oorsprong instellen",
        L"Midden van actief object",
        L"Midden van het printbed",
        L"OORSPRONG",
        L"gebaseerd op PrusaSlicer {0}",
        L"OpenSlicer-fout",
        L"PrusaSlicer.dll kan niet worden geladen:\n{0}\nFoutcode: {1}",
        L"Het startpunt slic3r_main is niet gevonden in {0}.",
        L"Deze PrusaSlicer.dll is niet de PrusaSlicer {0}-build waarvoor OpenSlicer is gemaakt.\nViewCube en 'Oorsprong instellen' zijn uitgeschakeld. Slicen en het tabblad Sync werken nog steeds."
    } },
    { L"pt_BR", {
        L"Filamentos instalados em {0}", L"Filamentos ativos em {0}", L"Ferramenta", L"Filamento",
        L"Alterar", L"Atribuir", L"Descarregar", L"Cor", L"Enviar para {0}", L"Atualizar", L"Cancelar", L"Aplicar",
        L"Nenhum material atribuído", L"Material desconhecido", L"Inativo", L"Nenhuma impressora",
        L"Ociosa", L"Pronta", L"Imprimindo", L"Ocupada", L"Offline",
        L"Lendo o Prusa Connect…",
        L"Sessão não iniciada. Entre pela aba Prusa Connect.",
        L"O Prusa Connect está inacessível.",
        L"Sessão expirada. Entre novamente pela aba Prusa Connect.",
        L"Erro do Prusa Connect (HTTP {0}).",
        L"Nenhuma impressora encontrada nesta conta.",
        L"{0}: {1} de {2} ferramentas carregadas.",
        L"{0} filamentos enviados para {1}.",
        L"Enviando para {0}…",
        L"Filamentos enviados para {0}.",
        L"Impressora inacessível na rede local.",
        L"Não enviado: a impressora está ocupada ({0}).",
        L"Falha no envio (HTTP {0}).",
        L"Nenhum filamento para enviar.",
        L"Material atribuído: {0}",
        L"Falha na atribuição: {0}",
        L"Descarregamento solicitado para {0}.",
        L"Falha no descarregamento: {0}",
        L"Atribuir um material a {0}",
        L"Pesquisar materiais, marcas ou tipos…",
        L"Carregando…", L"Nenhum material encontrado.", L"Não foi possível carregar os materiais.", L"Material personalizado",
        L"Descarregar o filamento de {0}? A impressora aquecerá o bico.",
        L"Definir origem",
        L"Centro do objeto ativo",
        L"Centro da mesa",
        L"ORIGEM",
        L"baseado no PrusaSlicer {0}",
        L"Erro do OpenSlicer",
        L"Não foi possível carregar PrusaSlicer.dll:\n{0}\nCódigo de erro: {1}",
        L"O ponto de entrada slic3r_main não foi encontrado em {0}.",
        L"Este PrusaSlicer.dll não é a compilação do PrusaSlicer {0} para a qual o OpenSlicer foi feito.\nO ViewCube e \"Definir origem\" estão desativados. O fatiamento e a aba Sync continuam funcionando."
    } },
    { L"ru", {
        L"Филаменты, установленные в {0}", L"Активные филаменты в {0}", L"Инструмент", L"Филамент",
        L"Изменить", L"Назначить", L"Выгрузить", L"Цвет", L"Отправить в {0}", L"Обновить", L"Отмена", L"Применить",
        L"Материал не назначен", L"Неизвестный материал", L"Неактивен", L"Нет принтера",
        L"Простой", L"Готов", L"Печать", L"Занят", L"Не в сети",
        L"Чтение Prusa Connect…",
        L"Вход не выполнен. Войдите на вкладке Prusa Connect.",
        L"Prusa Connect недоступен.",
        L"Сеанс истёк. Войдите снова на вкладке Prusa Connect.",
        L"Ошибка Prusa Connect (HTTP {0}).",
        L"В этой учётной записи принтеры не найдены.",
        L"{0}: загружено инструментов: {1} из {2}.",
        L"Отправлено филаментов: {0} → {1}.",
        L"Отправка в {0}…",
        L"Филаменты отправлены в {0}.",
        L"Принтер недоступен в локальной сети.",
        L"Не отправлено: принтер занят ({0}).",
        L"Не удалось отправить (HTTP {0}).",
        L"Нет филамента для отправки.",
        L"Материал назначен: {0}",
        L"Не удалось назначить: {0}",
        L"Запрошена выгрузка: {0}.",
        L"Не удалось выгрузить: {0}",
        L"Назначить материал: {0}",
        L"Поиск материалов, брендов или типов…",
        L"Загрузка…", L"Материал не найден.", L"Не удалось загрузить материалы.", L"Пользовательский материал",
        L"Выгрузить филамент из «{0}»? Принтер нагреет сопло.",
        L"Задать центр вращения",
        L"Центр активного объекта",
        L"Центр стола",
        L"ЦЕНТР",
        L"на основе PrusaSlicer {0}",
        L"Ошибка OpenSlicer",
        L"Не удалось загрузить PrusaSlicer.dll:\n{0}\nКод ошибки: {1}",
        L"Точка входа slic3r_main не найдена в {0}.",
        L"Этот PrusaSlicer.dll не является сборкой PrusaSlicer {0}, для которой создан OpenSlicer.\nViewCube и «Задать центр вращения» отключены. Нарезка и вкладка Sync работают как обычно."
    } },
    { L"uk", {
        L"Філаменти, встановлені в {0}", L"Активні філаменти в {0}", L"Інструмент", L"Філамент",
        L"Змінити", L"Призначити", L"Вивантажити", L"Колір", L"Надіслати до {0}", L"Оновити", L"Скасувати", L"Застосувати",
        L"Матеріал не призначено", L"Невідомий матеріал", L"Неактивний", L"Немає принтера",
        L"Простій", L"Готовий", L"Друк", L"Зайнятий", L"Не в мережі",
        L"Читання Prusa Connect…",
        L"Вхід не виконано. Увійдіть на вкладці Prusa Connect.",
        L"Prusa Connect недоступний.",
        L"Сеанс завершився. Увійдіть знову на вкладці Prusa Connect.",
        L"Помилка Prusa Connect (HTTP {0}).",
        L"У цьому обліковому записі принтерів не знайдено.",
        L"{0}: завантажено інструментів: {1} з {2}.",
        L"Надіслано філаментів: {0} → {1}.",
        L"Надсилання до {0}…",
        L"Філаменти надіслано до {0}.",
        L"Принтер недоступний у локальній мережі.",
        L"Не надіслано: принтер зайнятий ({0}).",
        L"Не вдалося надіслати (HTTP {0}).",
        L"Немає філаменту для надсилання.",
        L"Матеріал призначено: {0}",
        L"Не вдалося призначити: {0}",
        L"Запитано вивантаження: {0}.",
        L"Не вдалося вивантажити: {0}",
        L"Призначити матеріал: {0}",
        L"Пошук матеріалів, брендів або типів…",
        L"Завантаження…", L"Матеріал не знайдено.", L"Не вдалося завантажити матеріали.", L"Власний матеріал",
        L"Вивантажити філамент із «{0}»? Принтер нагріє сопло.",
        L"Задати центр обертання",
        L"Центр активного об'єкта",
        L"Центр столу",
        L"ЦЕНТР",
        L"на основі PrusaSlicer {0}",
        L"Помилка OpenSlicer",
        L"Не вдалося завантажити PrusaSlicer.dll:\n{0}\nКод помилки: {1}",
        L"Точку входу slic3r_main не знайдено в {0}.",
        L"Цей PrusaSlicer.dll не є збіркою PrusaSlicer {0}, для якої створено OpenSlicer.\nViewCube і «Задати центр обертання» вимкнено. Нарізання та вкладка Sync працюють як зазвичай."
    } },
    { L"be", {
        L"Філаменты, устаноўленыя ў {0}", L"Актыўныя філаменты ў {0}", L"Інструмент", L"Філамент",
        L"Змяніць", L"Прызначыць", L"Выгрузіць", L"Колер", L"Адправіць у {0}", L"Абнавіць", L"Скасаваць", L"Ужыць",
        L"Матэрыял не прызначаны", L"Невядомы матэрыял", L"Неактыўны", L"Няма прынтара",
        L"Прастой", L"Гатовы", L"Друк", L"Заняты", L"Па-за сеткай",
        L"Чытанне Prusa Connect…",
        L"Уваход не выкананы. Увайдзіце на ўкладцы Prusa Connect.",
        L"Prusa Connect недаступны.",
        L"Сеанс скончыўся. Увайдзіце зноў на ўкладцы Prusa Connect.",
        L"Памылка Prusa Connect (HTTP {0}).",
        L"У гэтым уліковым запісе прынтары не знойдзены.",
        L"{0}: загружана інструментаў: {1} з {2}.",
        L"Адпраўлена філаментаў: {0} → {1}.",
        L"Адпраўка ў {0}…",
        L"Філаменты адпраўлены ў {0}.",
        L"Прынтар недаступны ў лакальнай сетцы.",
        L"Не адпраўлена: прынтар заняты ({0}).",
        L"Не ўдалося адправіць (HTTP {0}).",
        L"Няма філамента для адпраўкі.",
        L"Матэрыял прызначаны: {0}",
        L"Не ўдалося прызначыць: {0}",
        L"Запытана выгрузка: {0}.",
        L"Не ўдалося выгрузіць: {0}",
        L"Прызначыць матэрыял: {0}",
        L"Пошук матэрыялаў, брэндаў або тыпаў…",
        L"Загрузка…", L"Матэрыял не знойдзены.", L"Не ўдалося загрузіць матэрыялы.", L"Уласны матэрыял",
        L"Выгрузіць філамент з «{0}»? Прынтар нагрэе сапло.",
        L"Задаць цэнтр вярчэння",
        L"Цэнтр актыўнага аб'екта",
        L"Цэнтр стала",
        L"ЦЭНТР",
        L"на аснове PrusaSlicer {0}",
        L"Памылка OpenSlicer",
        L"Не ўдалося загрузіць PrusaSlicer.dll:\n{0}\nКод памылкі: {1}",
        L"Кропка ўваходу slic3r_main не знойдзена ў {0}.",
        L"Гэты PrusaSlicer.dll не з'яўляецца зборкай PrusaSlicer {0}, для якой створаны OpenSlicer.\nViewCube і «Задаць цэнтр вярчэння» адключаны. Нарэзка і ўкладка Sync працуюць як звычайна."
    } },
    { L"ca", {
        L"Filaments instal·lats a {0}", L"Filaments actius a {0}", L"Eina", L"Filament",
        L"Canvia", L"Assigna", L"Descarrega", L"Color", L"Envia a {0}", L"Actualitza", L"Cancel·la", L"Aplica",
        L"Cap material assignat", L"Material desconegut", L"Inactiu", L"Cap impressora",
        L"En repòs", L"A punt", L"Imprimint", L"Ocupada", L"Fora de línia",
        L"S'està llegint Prusa Connect…",
        L"Sessió no iniciada. Inicia la sessió des de la pestanya Prusa Connect.",
        L"No es pot accedir a Prusa Connect.",
        L"Sessió caducada. Torna a iniciar la sessió des de la pestanya Prusa Connect.",
        L"Error de Prusa Connect (HTTP {0}).",
        L"No s'ha trobat cap impressora en aquest compte.",
        L"{0}: {1} de {2} eines carregades.",
        L"{0} filaments enviats a {1}.",
        L"S'està enviant a {0}…",
        L"Filaments enviats a {0}.",
        L"Impressora inaccessible a la xarxa local.",
        L"No enviat: la impressora està ocupada ({0}).",
        L"L'enviament ha fallat (HTTP {0}).",
        L"Cap filament per enviar.",
        L"Material assignat: {0}",
        L"L'assignació ha fallat: {0}",
        L"Descàrrega sol·licitada per a {0}.",
        L"La descàrrega ha fallat: {0}",
        L"Assigna un material a {0}",
        L"Cerca materials, marques o tipus…",
        L"S'està carregant…", L"No s'ha trobat cap material.", L"No s'han pogut carregar els materials.", L"Material personalitzat",
        L"Vols descarregar el filament de {0}? La impressora escalfarà el broquet.",
        L"Defineix l'origen",
        L"Centre de l'objecte actiu",
        L"Centre del llit",
        L"ORIGEN",
        L"basat en PrusaSlicer {0}",
        L"Error d'OpenSlicer",
        L"No es pot carregar PrusaSlicer.dll:\n{0}\nCodi d'error: {1}",
        L"No s'ha trobat el punt d'entrada slic3r_main a {0}.",
        L"Aquest PrusaSlicer.dll no és la compilació de PrusaSlicer {0} per a la qual es va fer OpenSlicer.\nEl ViewCube i «Defineix l'origen» estan desactivats. El laminat i la pestanya Sync continuen funcionant."
    } },
    { L"fi", {
        L"Asennetut filamentit: {0}", L"Aktiiviset filamentit: {0}", L"Työkalu", L"Filamentti",
        L"Muuta", L"Määritä", L"Poista", L"Väri", L"Lähetä: {0}", L"Päivitä", L"Peruuta", L"Käytä",
        L"Ei määritettyä materiaalia", L"Tuntematon materiaali", L"Ei käytössä", L"Ei tulostinta",
        L"Vapaa", L"Valmis", L"Tulostaa", L"Varattu", L"Offline",
        L"Luetaan Prusa Connectia…",
        L"Et ole kirjautunut. Kirjaudu Prusa Connect -välilehdellä.",
        L"Prusa Connectiin ei saada yhteyttä.",
        L"Istunto vanhentui. Kirjaudu uudelleen Prusa Connect -välilehdellä.",
        L"Prusa Connect -virhe (HTTP {0}).",
        L"Tililtä ei löytynyt tulostinta.",
        L"{0}: ladattuja työkaluja {1}/{2}.",
        L"Lähetetyt filamentit ({0}): {1}.",
        L"Lähetetään: {0}…",
        L"Filamentit lähetetty: {0}.",
        L"Tulostimeen ei saada yhteyttä lähiverkossa.",
        L"Ei lähetetty: tulostin on varattu ({0}).",
        L"Lähetys epäonnistui (HTTP {0}).",
        L"Ei lähetettävää filamenttia.",
        L"Materiaali määritetty: {0}",
        L"Määritys epäonnistui: {0}",
        L"Poisto pyydetty: {0}.",
        L"Poisto epäonnistui: {0}",
        L"Määritä materiaali: {0}",
        L"Hae materiaaleja, merkkejä tai tyyppejä…",
        L"Ladataan…", L"Materiaalia ei löytynyt.", L"Materiaaleja ei voitu ladata.", L"Mukautettu materiaali",
        L"Poistetaanko filamentti: {0}? Tulostin lämmittää suuttimen.",
        L"Aseta origo",
        L"Aktiivisen kappaleen keskipiste",
        L"Tulostusalustan keskipiste",
        L"ORIGO",
        L"pohjana PrusaSlicer {0}",
        L"OpenSlicer-virhe",
        L"PrusaSlicer.dll-tiedostoa ei voitu ladata:\n{0}\nVirhekoodi: {1}",
        L"Aloituspistettä slic3r_main ei löytynyt tiedostosta {0}.",
        L"Tämä PrusaSlicer.dll ei ole PrusaSlicer {0} -koontiversio, jota varten OpenSlicer on tehty.\nViewCube ja Aseta origo ovat pois käytöstä. Viipalointi ja Sync-välilehti toimivat edelleen."
    } },
    { L"hu", {
        L"Betöltött filamentek: {0}", L"Aktív filamentek: {0}", L"Szerszám", L"Filament",
        L"Módosítás", L"Hozzárendelés", L"Kiürítés", L"Szín", L"Küldés ide: {0}", L"Frissítés", L"Mégse", L"Alkalmaz",
        L"Nincs hozzárendelt anyag", L"Ismeretlen anyag", L"Inaktív", L"Nincs nyomtató",
        L"Tétlen", L"Kész", L"Nyomtat", L"Foglalt", L"Offline",
        L"Prusa Connect olvasása…",
        L"Nincs bejelentkezve. Jelentkezzen be a Prusa Connect lapon.",
        L"A Prusa Connect nem érhető el.",
        L"A munkamenet lejárt. Jelentkezzen be újra a Prusa Connect lapon.",
        L"Prusa Connect hiba (HTTP {0}).",
        L"Ebben a fiókban nem található nyomtató.",
        L"{0}: betöltött szerszámok: {1} / {2}.",
        L"Elküldött filamentek ({0}): {1}.",
        L"Küldés ide: {0}…",
        L"Filamentek elküldve ide: {0}.",
        L"A nyomtató nem érhető el a helyi hálózaton.",
        L"Nincs elküldve: a nyomtató foglalt ({0}).",
        L"A küldés sikertelen (HTTP {0}).",
        L"Nincs küldhető filament.",
        L"Anyag hozzárendelve: {0}",
        L"A hozzárendelés sikertelen: {0}",
        L"Kiürítés kérve: {0}.",
        L"A kiürítés sikertelen: {0}",
        L"Anyag hozzárendelése: {0}",
        L"Anyagok, márkák vagy típusok keresése…",
        L"Betöltés…", L"Nem található anyag.", L"Az anyagok nem tölthetők be.", L"Egyéni anyag",
        L"Kiüríti a filamentet innen: {0}? A nyomtató felfűti a fúvókát.",
        L"Origó beállítása",
        L"Az aktív objektum középpontja",
        L"A tárgyasztal középpontja",
        L"ORIGÓ",
        L"PrusaSlicer {0} alapján",
        L"OpenSlicer hiba",
        L"A PrusaSlicer.dll nem tölthető be:\n{0}\nHibakód: {1}",
        L"A slic3r_main belépési pont nem található: {0}.",
        L"Ez a PrusaSlicer.dll nem az a PrusaSlicer {0} build, amelyhez az OpenSlicer készült.\nA ViewCube és az Origó beállítása ki van kapcsolva. A szeletelés és a Sync lap továbbra is működik."
    } },
    { L"ja", {
        L"{0} に装着されたフィラメント", L"{0} で有効なフィラメント", L"ツール", L"フィラメント",
        L"変更", L"割り当て", L"アンロード", L"色", L"{0} に送信", L"更新", L"キャンセル", L"適用",
        L"材料が割り当てられていません", L"不明な材料", L"無効", L"プリンターなし",
        L"待機中", L"準備完了", L"印刷中", L"ビジー", L"オフライン",
        L"Prusa Connect を読み込み中…",
        L"ログインしていません。Prusa Connect タブからログインしてください。",
        L"Prusa Connect に接続できません。",
        L"セッションの有効期限が切れました。Prusa Connect タブから再度ログインしてください。",
        L"Prusa Connect エラー (HTTP {0})。",
        L"このアカウントにプリンターが見つかりません。",
        L"{0}: {2} 個中 {1} 個のツールがロード済み。",
        L"{0} 個のフィラメントを {1} に送信しました。",
        L"{0} に送信中…",
        L"フィラメントを {0} に送信しました。",
        L"ローカルネットワーク上でプリンターに接続できません。",
        L"未送信: プリンターがビジーです ({0})。",
        L"送信に失敗しました (HTTP {0})。",
        L"送信するフィラメントがありません。",
        L"材料を割り当てました: {0}",
        L"割り当てに失敗しました: {0}",
        L"{0} のアンロードを要求しました。",
        L"アンロードに失敗しました: {0}",
        L"{0} に材料を割り当て",
        L"材料、ブランド、種類を検索…",
        L"読み込み中…", L"材料が見つかりません。", L"材料を読み込めませんでした。", L"カスタム材料",
        L"{0} のフィラメントをアンロードしますか？ プリンターがノズルを加熱します。",
        L"原点を設定",
        L"アクティブなオブジェクトの中心",
        L"ベッドの中心",
        L"原点",
        L"PrusaSlicer {0} ベース",
        L"OpenSlicer エラー",
        L"PrusaSlicer.dll を読み込めません:\n{0}\nエラーコード: {1}",
        L"{0} にエントリポイント slic3r_main が見つかりません。",
        L"この PrusaSlicer.dll は、OpenSlicer の対象である PrusaSlicer {0} のビルドではありません。\nViewCube と「原点を設定」は無効です。スライスと Sync タブは引き続き使用できます。"
    } },
    { L"ko", {
        L"{0}에 장착된 필라멘트", L"{0}의 활성 필라멘트", L"툴", L"필라멘트",
        L"변경", L"할당", L"언로드", L"색상", L"{0}(으)로 보내기", L"새로 고침", L"취소", L"적용",
        L"할당된 재료 없음", L"알 수 없는 재료", L"비활성", L"프린터 없음",
        L"대기", L"준비됨", L"출력 중", L"사용 중", L"오프라인",
        L"Prusa Connect 읽는 중…",
        L"로그인되어 있지 않습니다. Prusa Connect 탭에서 로그인하세요.",
        L"Prusa Connect에 연결할 수 없습니다.",
        L"세션이 만료되었습니다. Prusa Connect 탭에서 다시 로그인하세요.",
        L"Prusa Connect 오류 (HTTP {0}).",
        L"이 계정에서 프린터를 찾을 수 없습니다.",
        L"{0}: 툴 {2}개 중 {1}개 로드됨.",
        L"필라멘트 {0}개를 {1}(으)로 보냈습니다.",
        L"{0}(으)로 보내는 중…",
        L"필라멘트를 {0}(으)로 보냈습니다.",
        L"로컬 네트워크에서 프린터에 연결할 수 없습니다.",
        L"보내지 않음: 프린터가 사용 중입니다 ({0}).",
        L"보내기 실패 (HTTP {0}).",
        L"보낼 필라멘트가 없습니다.",
        L"재료 할당됨: {0}",
        L"할당 실패: {0}",
        L"{0} 언로드를 요청했습니다.",
        L"언로드 실패: {0}",
        L"{0}에 재료 할당",
        L"재료, 브랜드 또는 유형 검색…",
        L"불러오는 중…", L"재료를 찾을 수 없습니다.", L"재료를 불러올 수 없습니다.", L"사용자 지정 재료",
        L"{0}의 필라멘트를 언로드할까요? 프린터가 노즐을 가열합니다.",
        L"원점 설정",
        L"활성 개체의 중심",
        L"베드 중심",
        L"원점",
        L"PrusaSlicer {0} 기반",
        L"OpenSlicer 오류",
        L"PrusaSlicer.dll을 불러올 수 없습니다:\n{0}\n오류 코드: {1}",
        L"{0}에서 slic3r_main 진입점을 찾을 수 없습니다.",
        L"이 PrusaSlicer.dll은 OpenSlicer가 대상으로 하는 PrusaSlicer {0} 빌드가 아닙니다.\nViewCube와 원점 설정이 비활성화됩니다. 슬라이싱과 Sync 탭은 계속 작동합니다."
    } },
    { L"sl", {
        L"Filamenti, nameščeni v {0}", L"Aktivni filamenti v {0}", L"Orodje", L"Filament",
        L"Spremeni", L"Dodeli", L"Odstrani", L"Barva", L"Pošlji v {0}", L"Osveži", L"Prekliči", L"Uporabi",
        L"Ni dodeljenega materiala", L"Neznan material", L"Neaktiven", L"Ni tiskalnika",
        L"Nedejaven", L"Pripravljen", L"Tiska", L"Zaseden", L"Brez povezave",
        L"Branje iz Prusa Connect…",
        L"Niste prijavljeni. Prijavite se na zavihku Prusa Connect.",
        L"Prusa Connect ni dosegljiv.",
        L"Seja je potekla. Znova se prijavite na zavihku Prusa Connect.",
        L"Napaka Prusa Connect (HTTP {0}).",
        L"V tem računu ni najdenega tiskalnika.",
        L"{0}: naložena orodja: {1} od {2}.",
        L"Poslani filamenti ({0}): {1}.",
        L"Pošiljanje v {0}…",
        L"Filamenti poslani v {0}.",
        L"Tiskalnik ni dosegljiv v lokalnem omrežju.",
        L"Ni poslano: tiskalnik je zaseden ({0}).",
        L"Pošiljanje ni uspelo (HTTP {0}).",
        L"Ni filamenta za pošiljanje.",
        L"Material dodeljen: {0}",
        L"Dodelitev ni uspela: {0}",
        L"Zahtevana odstranitev: {0}.",
        L"Odstranitev ni uspela: {0}",
        L"Dodeli material: {0}",
        L"Iskanje materialov, znamk ali vrst…",
        L"Nalaganje…", L"Ni najdenega materiala.", L"Materialov ni bilo mogoče naložiti.", L"Material po meri",
        L"Odstranim filament iz: {0}? Tiskalnik bo segrel šobo.",
        L"Nastavi izhodišče",
        L"Središče aktivnega predmeta",
        L"Središče podlage",
        L"IZHODIŠČE",
        L"na osnovi PrusaSlicer {0}",
        L"Napaka OpenSlicer",
        L"PrusaSlicer.dll ni mogoče naložiti:\n{0}\nKoda napake: {1}",
        L"Vstopne točke slic3r_main ni v {0}.",
        L"Ta PrusaSlicer.dll ni različica PrusaSlicer {0}, za katero je narejen OpenSlicer.\nViewCube in Nastavi izhodišče sta onemogočena. Rezanje in zavihek Sync delujeta normalno."
    } },
    { L"tr", {
        L"{0} içinde takılı filamentler", L"{0} içindeki etkin filamentler", L"Araç", L"Filament",
        L"Değiştir", L"Ata", L"Boşalt", L"Renk", L"{0} hedefine gönder", L"Yenile", L"İptal", L"Uygula",
        L"Atanmış malzeme yok", L"Bilinmeyen malzeme", L"Etkin değil", L"Yazıcı yok",
        L"Boşta", L"Hazır", L"Yazdırıyor", L"Meşgul", L"Çevrimdışı",
        L"Prusa Connect okunuyor…",
        L"Oturum açılmadı. Prusa Connect sekmesinden oturum açın.",
        L"Prusa Connect'e ulaşılamıyor.",
        L"Oturumun süresi doldu. Prusa Connect sekmesinden yeniden oturum açın.",
        L"Prusa Connect hatası (HTTP {0}).",
        L"Bu hesapta yazıcı bulunamadı.",
        L"{0}: {2} araçtan {1} tanesi yüklü.",
        L"{0} filament {1} hedefine gönderildi.",
        L"{0} hedefine gönderiliyor…",
        L"Filamentler {0} hedefine gönderildi.",
        L"Yazıcıya yerel ağda ulaşılamıyor.",
        L"Gönderilmedi: yazıcı meşgul ({0}).",
        L"Gönderme başarısız (HTTP {0}).",
        L"Gönderilecek filament yok.",
        L"Malzeme atandı: {0}",
        L"Atama başarısız: {0}",
        L"{0} için boşaltma istendi.",
        L"Boşaltma başarısız: {0}",
        L"{0} için malzeme ata",
        L"Malzeme, marka veya tür ara…",
        L"Yükleniyor…", L"Malzeme bulunamadı.", L"Malzemeler yüklenemedi.", L"Özel malzeme",
        L"{0} filamenti boşaltılsın mı? Yazıcı nozülü ısıtacak.",
        L"Orijini ayarla",
        L"Etkin nesnenin merkezi",
        L"Tablanın merkezi",
        L"ORİJİN",
        L"PrusaSlicer {0} tabanlı",
        L"OpenSlicer hatası",
        L"PrusaSlicer.dll yüklenemedi:\n{0}\nHata kodu: {1}",
        L"{0} içinde slic3r_main giriş noktası bulunamadı.",
        L"Bu PrusaSlicer.dll, OpenSlicer'ın hedeflediği PrusaSlicer {0} derlemesi değil.\nViewCube ve Orijini ayarla devre dışı. Dilimleme ve Sync sekmesi çalışmaya devam eder."
    } },
    { L"zh_CN", {
        L"{0} 中已安装的耗材", L"{0} 中的活动耗材", L"工具", L"耗材",
        L"更改", L"分配", L"退料", L"颜色", L"发送到 {0}", L"刷新", L"取消", L"应用",
        L"未分配材料", L"未知材料", L"未启用", L"无打印机",
        L"空闲", L"就绪", L"打印中", L"忙碌", L"离线",
        L"正在读取 Prusa Connect…",
        L"未登录。请在 Prusa Connect 选项卡中登录。",
        L"无法连接 Prusa Connect。",
        L"会话已过期。请在 Prusa Connect 选项卡中重新登录。",
        L"Prusa Connect 错误 (HTTP {0})。",
        L"此账户中未找到打印机。",
        L"{0}：已装载 {1}/{2} 个工具。",
        L"已将 {0} 个耗材发送到 {1}。",
        L"正在发送到 {0}…",
        L"耗材已发送到 {0}。",
        L"在本地网络上无法连接打印机。",
        L"未发送：打印机正忙 ({0})。",
        L"发送失败 (HTTP {0})。",
        L"没有可发送的耗材。",
        L"已分配材料：{0}",
        L"分配失败：{0}",
        L"已请求为 {0} 退料。",
        L"退料失败：{0}",
        L"为 {0} 分配材料",
        L"搜索材料、品牌或类型…",
        L"正在加载…", L"未找到材料。", L"无法加载材料。", L"自定义材料",
        L"要从 {0} 退出耗材吗？打印机将加热喷嘴。",
        L"设置原点",
        L"活动对象中心",
        L"热床中心",
        L"原点",
        L"基于 PrusaSlicer {0}",
        L"OpenSlicer 错误",
        L"无法加载 PrusaSlicer.dll：\n{0}\n错误代码：{1}",
        L"在 {0} 中找不到入口点 slic3r_main。",
        L"此 PrusaSlicer.dll 不是 OpenSlicer 所针对的 PrusaSlicer {0} 版本。\nViewCube 和“设置原点”已禁用。切片和 Sync 选项卡仍可正常使用。"
    } },
    { L"zh_TW", {
        L"{0} 中已安裝的線材", L"{0} 中的使用中線材", L"工具", L"線材",
        L"變更", L"指派", L"退料", L"顏色", L"傳送到 {0}", L"重新整理", L"取消", L"套用",
        L"未指派材料", L"未知材料", L"未啟用", L"沒有印表機",
        L"閒置", L"就緒", L"列印中", L"忙碌", L"離線",
        L"正在讀取 Prusa Connect…",
        L"尚未登入。請在 Prusa Connect 分頁中登入。",
        L"無法連線到 Prusa Connect。",
        L"工作階段已過期。請在 Prusa Connect 分頁中重新登入。",
        L"Prusa Connect 錯誤 (HTTP {0})。",
        L"此帳戶中找不到印表機。",
        L"{0}：已裝載 {1}/{2} 個工具。",
        L"已將 {0} 個線材傳送到 {1}。",
        L"正在傳送到 {0}…",
        L"線材已傳送到 {0}。",
        L"在區域網路上無法連線到印表機。",
        L"未傳送：印表機忙碌中 ({0})。",
        L"傳送失敗 (HTTP {0})。",
        L"沒有可傳送的線材。",
        L"已指派材料：{0}",
        L"指派失敗：{0}",
        L"已要求為 {0} 退料。",
        L"退料失敗：{0}",
        L"為 {0} 指派材料",
        L"搜尋材料、品牌或類型…",
        L"載入中…", L"找不到材料。", L"無法載入材料。", L"自訂材料",
        L"要從 {0} 退出線材嗎？印表機將加熱噴嘴。",
        L"設定原點",
        L"作用中物件中心",
        L"熱床中心",
        L"原點",
        L"基於 PrusaSlicer {0}",
        L"OpenSlicer 錯誤",
        L"無法載入 PrusaSlicer.dll：\n{0}\n錯誤代碼：{1}",
        L"在 {0} 中找不到進入點 slic3r_main。",
        L"此 PrusaSlicer.dll 不是 OpenSlicer 所針對的 PrusaSlicer {0} 版本。\nViewCube 與「設定原點」已停用。切片與 Sync 分頁仍可正常使用。"
    } }
};

inline constexpr int kLangCount = (int)(sizeof(kLangs) / sizeof(kLangs[0]));

// ----------------------------------------------------------------------------
// Active language: "translation_language" of PrusaSlicer.ini, else the Windows UI language
// (same resolution order as GUI_App::load_language).
// ----------------------------------------------------------------------------
inline std::wstring PrusaSlicerIniPath() {
    wchar_t appData[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData))) return L"";
    return std::wstring(appData) + L"\\PrusaSlicer\\PrusaSlicer.ini";
}

inline std::wstring ReadIniTranslationLanguage() {
    std::wstring path = PrusaSlicerIniPath();
    if (path.empty()) return L"";
    FILE* f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"rb") != 0 || !f) return L"";
    std::wstring found;
    char line[1024];
    int guard = 0;
    while (fgets(line, sizeof(line), f) && guard++ < 4000) {
        const char* p = line;
        while (*p == ' ' || *p == '\t') ++p;
        static const char kKey[] = "translation_language";
        if (strncmp(p, kKey, sizeof(kKey) - 1) != 0) continue;
        p += sizeof(kKey) - 1;
        while (*p == ' ' || *p == '\t') ++p;
        if (*p != '=') continue;
        ++p;
        while (*p == ' ' || *p == '\t') ++p;
        while (*p && *p != '\r' && *p != '\n' && *p != ' ') {
            found.push_back((wchar_t)(unsigned char)*p);
            ++p;
        }
        break;
    }
    fclose(f);
    return found;
}

inline int FindLangIndex(std::wstring code) {
    for (auto& c : code) {
        if (c == L'-') c = L'_';
        else if (c >= L'A' && c <= L'Z') c = (wchar_t)(c - L'A' + L'a');
    }
    if (code.empty()) return -1;
    auto starts = [&](const wchar_t* p) { return code.rfind(p, 0) == 0; };
    const wchar_t* want = nullptr;
    if (starts(L"zh")) {
        bool traditional = code.find(L"tw") != std::wstring::npos || code.find(L"hk") != std::wstring::npos ||
                           code.find(L"mo") != std::wstring::npos || code.find(L"hant") != std::wstring::npos;
        want = traditional ? L"zh_TW" : L"zh_CN";
    } else if (starts(L"pt")) {
        want = L"pt_BR";
    }
    std::wstring two = code.substr(0, 2);
    for (int i = 0; i < kLangCount; ++i) {
        if (want ? (wcscmp(kLangs[i].code, want) == 0) : (two == kLangs[i].code)) return i;
    }
    return -1;
}

inline int DetectLangIndex() {
    // Optional override for testing translations: set OPENPS_SYNC_LANG=de (or ru, ja, zh_CN...)
    wchar_t forced[32]{};
    DWORD forcedLen = GetEnvironmentVariableW(L"OPENPS_SYNC_LANG", forced, 31);
    if (forcedLen > 0 && forcedLen < 31) {
        int forcedIdx = FindLangIndex(forced);
        if (forcedIdx >= 0) return forcedIdx;
    }
    int idx = FindLangIndex(ReadIniTranslationLanguage());
    if (idx >= 0) return idx;
    wchar_t name[LOCALE_NAME_MAX_LENGTH]{};
    if (LCIDToLocaleName(MAKELCID(GetUserDefaultUILanguage(), SORT_DEFAULT), name, LOCALE_NAME_MAX_LENGTH, 0) > 0) {
        idx = FindLangIndex(name);
        if (idx >= 0) return idx;
    }
    return 0; // English
}

inline int& ActiveLangIndex() {
    static int idx = DetectLangIndex();
    return idx;
}

// Incremented each time the language changes, so that cached labels can be rebuilt
inline int& LanguageGeneration() {
    static int gen = 0;
    return gen;
}

inline const wchar_t* Tr(StrId id) {
    if (id < 0 || id >= S_COUNT) return L"";
    const wchar_t* s = kLangs[ActiveLangIndex()].s[id];
    return (s && *s) ? s : kLangs[0].s[id];
}

// Replaces {0}, {1}, {2} placeholders
inline std::wstring Fmt(StrId id, std::initializer_list<std::wstring> args) {
    std::wstring out = Tr(id);
    int n = 0;
    for (const auto& a : args) {
        wchar_t ph[8]{};
        swprintf_s(ph, L"{%d}", n++);
        size_t pos = 0;
        while ((pos = out.find(ph, pos)) != std::wstring::npos) {
            out.replace(pos, wcslen(ph), a);
            pos += a.size();
        }
    }
    return out;
}

inline const wchar_t* ActiveLangCode() {
    return kLangs[ActiveLangIndex()].code;
}

// ----------------------------------------------------------------------------
// PrusaSlicer's own gettext catalog (resources/localization/<lang>/PrusaSlicer.mo next to the exe).
// Used for strings PrusaSlicer already translates ("Top", "Front", "Add Shape"...) so that
// OpenSlicer shows exactly the same words as the rest of the application.
// ----------------------------------------------------------------------------
class AppCatalog {
public:
    static AppCatalog& Instance() {
        static AppCatalog inst;
        return inst;
    }

    void Reload() {
        m_entries.clear();
        wchar_t exe[MAX_PATH]{};
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        std::wstring dir(exe);
        size_t slash = dir.find_last_of(L"\\/");
        dir = (slash == std::wstring::npos) ? L"." : dir.substr(0, slash);
        Load(dir + L"\\resources\\localization\\" + ActiveLangCode() + L"\\PrusaSlicer.mo");
    }

    // Translation of msgid, or msgid itself when the catalog has no entry
    std::wstring Get(const char* msgid) const {
        auto it = m_entries.find(msgid);
        std::string out = (it != m_entries.end() && !it->second.empty()) ? it->second : std::string(msgid);
        // Some translators prefix a disambiguation context ("Layer||Bottom"): keep the last part
        size_t ctx = out.rfind("||");
        if (ctx != std::string::npos) out = out.substr(ctx + 2);
        // Drop menu mnemonics ("&Top" -> "Top")
        std::string clean;
        for (size_t i = 0; i < out.size(); ++i) {
            if (out[i] == '&') {
                if (i + 1 < out.size() && out[i + 1] == '&') { clean.push_back('&'); ++i; }
                continue;
            }
            clean.push_back(out[i]);
        }
        int n = MultiByteToWideChar(CP_UTF8, 0, clean.data(), (int)clean.size(), nullptr, 0);
        std::wstring w((size_t)std::max(0, n), L'\0');
        if (n > 0) MultiByteToWideChar(CP_UTF8, 0, clean.data(), (int)clean.size(), &w[0], n);
        return w;
    }

private:
    std::map<std::string, std::string> m_entries;

    AppCatalog() { Reload(); }

    // GNU gettext .mo format: magic, revision, count, offset of original table, offset of translation table
    void Load(const std::wstring& path) {
        FILE* f = nullptr;
        if (_wfopen_s(&f, path.c_str(), L"rb") != 0 || !f) return;
        std::string data;
        char buf[65536];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0) data.append(buf, n);
        fclose(f);
        if (data.size() < 28) return;

        auto u32 = [&](size_t off) -> uint32_t {
            if (off + 4 > data.size()) return 0;
            const unsigned char* p = reinterpret_cast<const unsigned char*>(data.data()) + off;
            return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
        };
        if (u32(0) != 0x950412DEu) return; // big-endian catalogs are not produced by PrusaSlicer
        uint32_t count = u32(8), origTab = u32(12), transTab = u32(16);
        for (uint32_t i = 0; i < count; ++i) {
            uint32_t ol = u32(origTab + i * 8), oo = u32(origTab + i * 8 + 4);
            uint32_t tl = u32(transTab + i * 8), to = u32(transTab + i * 8 + 4);
            if ((size_t)oo + ol > data.size() || (size_t)to + tl > data.size()) break;
            // Plural entries hold "singular\0plural": the singular is the key
            std::string key(data.data() + oo, strnlen(data.data() + oo, ol));
            std::string val(data.data() + to, strnlen(data.data() + to, tl));
            if (!key.empty()) m_entries.emplace(std::move(key), std::move(val));
        }
    }
};

inline std::wstring TrApp(const char* msgid) {
    return AppCatalog::Instance().Get(msgid);
}

// PrusaSlicer switches language without restarting (it recreates its main window) and saves
// PrusaSlicer.ini shortly after. Call this periodically: it re-reads the language when the ini
// file changed, and returns true when the language is different.
inline bool RefreshLanguage() {
    static FILETIME lastWrite{};
    WIN32_FILE_ATTRIBUTE_DATA fad{};
    std::wstring ini = PrusaSlicerIniPath();
    if (ini.empty() || !GetFileAttributesExW(ini.c_str(), GetFileExInfoStandard, &fad)) return false;
    if (CompareFileTime(&fad.ftLastWriteTime, &lastWrite) == 0) return false;
    lastWrite = fad.ftLastWriteTime;

    int idx = DetectLangIndex();
    if (idx == ActiveLangIndex()) return false;
    ActiveLangIndex() = idx;
    ++LanguageGeneration();
    AppCatalog::Instance().Reload();
    return true;
}

} // namespace openps::sync::i18n

#endif // OPENPS_SYNC_INDX_I18N_HPP
