/*************************************************************************/
/*  translation.cpp                                                      */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "translation.h"

#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/os/main_loop.h"
#include "core/os/os.h"
#include "core/pool_vector.h"
#include "core/print_string.h"
#include "core/project_settings.h"
#include "core/resource/resource_manager.h"
#include "core/script_language.h"
#include "core/string_formatter.h"

IMPL_GDCLASS(Translation)
IMPL_GDCLASS(TranslationServer)
IMPL_GDCLASS(ContextTranslation)
RES_BASE_EXTENSION_IMPL(Translation,"translation")

// Windows has some weird locale identifiers which do not honor the ISO 639-1
// standardized nomenclature. Whenever those don't conflict with existing ISO
// identifiers, we override them.
//
// Reference:
// - https://msdn.microsoft.com/en-us/library/windows/desktop/ms693062(v=vs.85).aspx

static const char *locale_renames[][2] = {
    { "in", "id" }, //  Indonesian
    { "iw", "he" }, //  Hebrew
    { "no", "nb" }, //  Norwegian Bokmål
    { "C", "en" }, // Locale is not set, fallback to English.
    { nullptr, nullptr }
};

// Additional script information to preferred scripts.
// Language code, script code, default country, supported countries.
// Reference:
// - https://lh.2xlibre.net/locales/
// - https://www.localeplanet.com/icu/index.html
// - https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-lcid/70feba9f-294e-491e-b6eb-56532684c37f

static const char *locale_scripts[][4] = {
    { "az", "Latn", "", "AZ" },
    { "az", "Arab", "", "IR" },
    { "bs", "Latn", "", "BA" },
    { "ff", "Latn", "", "BF,CM,GH,GM,GN,GW,LR,MR,NE,NG,SL,SN" },
    { "pa", "Arab", "PK", "PK" },
    { "pa", "Guru", "IN", "IN" },
    { "sd", "Arab", "PK", "PK" },
    { "sd", "Deva", "IN", "IN" },
    { "shi", "Tfng", "", "MA" },
    { "sr", "Cyrl", "", "BA,RS,XK" },
    { "sr", "Latn", "", "ME" },
    { "uz", "Latn", "", "UZ" },
    { "uz", "Arab", "AF", "AF" },
    { "vai", "Vaii", "", "LR" },
    { "yue", "Hans", "CN", "CN" },
    { "yue", "Hant", "HK", "HK" },
    { "zh", "Hans", "CN", "CN,SG" },
    { "zh", "Hant", "TW", "HK,MO,TW" },
    { nullptr, nullptr, nullptr, nullptr }
};

// Additional mapping for outdated, temporary or exceptionally reserved country codes.
// Reference:
// - https://en.wikipedia.org/wiki/ISO_3166-1_alpha-2
// - https://www.iso.org/obp/ui/#search/code/

static const char *country_renames[][2] = {
    { "BU", "MM" }, // Burma, name changed to Myanmar.
    { "KV", "XK" }, // Kosovo (temporary FIPS code to European Commission code), no official ISO code assigned.
    { "TP", "TL" }, // East Timor, name changed to Timor-Leste.
    { "UK", "GB" }, // United Kingdom, exceptionally reserved code.
    { nullptr, nullptr }
};

// Country code, country name.
// Reference:
// - https://en.wikipedia.org/wiki/ISO_3166-1_alpha-2
// - https://www.iso.org/obp/ui/#search/code/

static const char *country_names[][2] = {
    { "AC", "Ascension Island" }, // Exceptionally reserved.
    { "AD", "Andorra" },
    { "AE", "United Arab Emirates" },
    { "AF", "Afghanistan" },
    { "AG", "Antigua and Barbuda" },
    { "AI", "Anguilla" },
    { "AL", "Albania" },
    { "AM", "Armenia" },
    { "AN", "Netherlands Antilles" }, // Transitionally reserved, divided into BQ, CW and SX.
    { "AO", "Angola" },
    { "AQ", "Antarctica" },
    { "AR", "Argentina" },
    { "AS", "American Samoa" },
    { "AT", "Austria" },
    { "AU", "Australia" },
    { "AW", "Aruba" },
    { "AX", "Åland Islands" },
    { "AZ", "Azerbaijan" },
    { "BA", "Bosnia and Herzegovina" },
    { "BB", "Barbados" },
    { "BD", "Bangladesh" },
    { "BE", "Belgium" },
    { "BF", "Burkina Faso" },
    { "BG", "Bulgaria" },
    { "BH", "Bahrain" },
    { "BI", "Burundi" },
    { "BJ", "Benin" },
    { "BL", "St. Barthélemy" },
    { "BM", "Bermuda" },
    { "BN", "Brunei" },
    { "BO", "Bolivia" },
    { "BQ", "Caribbean Netherlands" },
    { "BR", "Brazil" },
    { "BS", "Bahamas" },
    { "BT", "Bhutan" },
    { "BV", "Bouvet Island" },
    { "BW", "Botswana" },
    { "BY", "Belarus" },
    { "BZ", "Belize" },
    { "CA", "Canada" },
    { "CC", "Cocos (Keeling) Islands" },
    { "CD", "Congo - Kinshasa" },
    { "CF", "Central African Republic" },
    { "CG", "Congo - Brazzaville" },
    { "CH", "Switzerland" },
    { "CI", "Côte d'Ivoire" },
    { "CK", "Cook Islands" },
    { "CL", "Chile" },
    { "CM", "Cameroon" },
    { "CN", "China" },
    { "CO", "Colombia" },
    { "CP", "Clipperton Island" }, // Exceptionally reserved.
    { "CR", "Costa Rica" },
    { "CQ", "Island of Sark" }, // Exceptionally reserved.
    { "CU", "Cuba" },
    { "CV", "Cabo Verde" },
    { "CW", "Curaçao" },
    { "CX", "Christmas Island" },
    { "CY", "Cyprus" },
    { "CZ", "Czechia" },
    { "DE", "Germany" },
    { "DG", "Diego Garcia" }, // Exceptionally reserved.
    { "DJ", "Djibouti" },
    { "DK", "Denmark" },
    { "DM", "Dominica" },
    { "DO", "Dominican Republic" },
    { "DZ", "Algeria" },
    { "EA", "Ceuta and Melilla" }, // Exceptionally reserved.
    { "EC", "Ecuador" },
    { "EE", "Estonia" },
    { "EG", "Egypt" },
    { "EH", "Western Sahara" },
    { "ER", "Eritrea" },
    { "ES", "Spain" },
    { "ET", "Ethiopia" },
    { "EU", "European Union" }, // Exceptionally reserved.
    { "EZ", "Eurozone" }, // Exceptionally reserved.
    { "FI", "Finland" },
    { "FJ", "Fiji" },
    { "FK", "Falkland Islands" },
    { "FM", "Micronesia" },
    { "FO", "Faroe Islands" },
    { "FR", "France" },
    { "FX", "France, Metropolitan" }, // Exceptionally reserved.
    { "GA", "Gabon" },
    { "GB", "United Kingdom" },
    { "GD", "Grenada" },
    { "GE", "Georgia" },
    { "GF", "French Guiana" },
    { "GG", "Guernsey" },
    { "GH", "Ghana" },
    { "GI", "Gibraltar" },
    { "GL", "Greenland" },
    { "GM", "Gambia" },
    { "GN", "Guinea" },
    { "GP", "Guadeloupe" },
    { "GQ", "Equatorial Guinea" },
    { "GR", "Greece" },
    { "GS", "South Georgia and South Sandwich Islands" },
    { "GT", "Guatemala" },
    { "GU", "Guam" },
    { "GW", "Guinea-Bissau" },
    { "GY", "Guyana" },
    { "HK", "Hong Kong" },
    { "HM", "Heard Island and McDonald Islands" },
    { "HN", "Honduras" },
    { "HR", "Croatia" },
    { "HT", "Haiti" },
    { "HU", "Hungary" },
    { "IC", "Canary Islands" }, // Exceptionally reserved.
    { "ID", "Indonesia" },
    { "IE", "Ireland" },
    { "IL", "Israel" },
    { "IM", "Isle of Man" },
    { "IN", "India" },
    { "IO", "British Indian Ocean Territory" },
    { "IQ", "Iraq" },
    { "IR", "Iran" },
    { "IS", "Iceland" },
    { "IT", "Italy" },
    { "JE", "Jersey" },
    { "JM", "Jamaica" },
    { "JO", "Jordan" },
    { "JP", "Japan" },
    { "KE", "Kenya" },
    { "KG", "Kyrgyzstan" },
    { "KH", "Cambodia" },
    { "KI", "Kiribati" },
    { "KM", "Comoros" },
    { "KN", "St. Kitts and Nevis" },
    { "KP", "North Korea" },
    { "KR", "South Korea" },
    { "KW", "Kuwait" },
    { "KY", "Cayman Islands" },
    { "KZ", "Kazakhstan" },
    { "LA", "Laos" },
    { "LB", "Lebanon" },
    { "LC", "St. Lucia" },
    { "LI", "Liechtenstein" },
    { "LK", "Sri Lanka" },
    { "LR", "Liberia" },
    { "LS", "Lesotho" },
    { "LT", "Lithuania" },
    { "LU", "Luxembourg" },
    { "LV", "Latvia" },
    { "LY", "Libya" },
    { "MA", "Morocco" },
    { "MC", "Monaco" },
    { "MD", "Moldova" },
    { "ME", "Montenegro" },
    { "MF", "St. Martin" },
    { "MG", "Madagascar" },
    { "MH", "Marshall Islands" },
    { "MK", "North Macedonia" },
    { "ML", "Mali" },
    { "MM", "Myanmar" },
    { "MN", "Mongolia" },
    { "MO", "Macao" },
    { "MP", "Northern Mariana Islands" },
    { "MQ", "Martinique" },
    { "MR", "Mauritania" },
    { "MS", "Montserrat" },
    { "MT", "Malta" },
    { "MU", "Mauritius" },
    { "MV", "Maldives" },
    { "MW", "Malawi" },
    { "MX", "Mexico" },
    { "MY", "Malaysia" },
    { "MZ", "Mozambique" },
    { "NA", "Namibia" },
    { "NC", "New Caledonia" },
    { "NE", "Niger" },
    { "NF", "Norfolk Island" },
    { "NG", "Nigeria" },
    { "NI", "Nicaragua" },
    { "NL", "Netherlands" },
    { "NO", "Norway" },
    { "NP", "Nepal" },
    { "NR", "Nauru" },
    { "NU", "Niue" },
    { "NZ", "New Zealand" },
    { "OM", "Oman" },
    { "PA", "Panama" },
    { "PE", "Peru" },
    { "PF", "French Polynesia" },
    { "PG", "Papua New Guinea" },
    { "PH", "Philippines" },
    { "PK", "Pakistan" },
    { "PL", "Poland" },
    { "PM", "St. Pierre and Miquelon" },
    { "PN", "Pitcairn Islands" },
    { "PR", "Puerto Rico" },
    { "PS", "Palestine" },
    { "PT", "Portugal" },
    { "PW", "Palau" },
    { "PY", "Paraguay" },
    { "QA", "Qatar" },
    { "RE", "Réunion" },
    { "RO", "Romania" },
    { "RS", "Serbia" },
    { "RU", "Russia" },
    { "RW", "Rwanda" },
    { "SA", "Saudi Arabia" },
    { "SB", "Solomon Islands" },
    { "SC", "Seychelles" },
    { "SD", "Sudan" },
    { "SE", "Sweden" },
    { "SG", "Singapore" },
    { "SH", "St. Helena, Ascension and Tristan da Cunha" },
    { "SI", "Slovenia" },
    { "SJ", "Svalbard and Jan Mayen" },
    { "SK", "Slovakia" },
    { "SL", "Sierra Leone" },
    { "SM", "San Marino" },
    { "SN", "Senegal" },
    { "SO", "Somalia" },
    { "SR", "Suriname" },
    { "SS", "South Sudan" },
    { "ST", "Sao Tome and Principe" },
    { "SV", "El Salvador" },
    { "SX", "Sint Maarten" },
    { "SY", "Syria" },
    { "SZ", "Eswatini" },
    { "TA", "Tristan da Cunha" }, // Exceptionally reserved.
    { "TC", "Turks and Caicos Islands" },
    { "TD", "Chad" },
    { "TF", "French Southern Territories" },
    { "TG", "Togo" },
    { "TH", "Thailand" },
    { "TJ", "Tajikistan" },
    { "TK", "Tokelau" },
    { "TL", "Timor-Leste" },
    { "TM", "Turkmenistan" },
    { "TN", "Tunisia" },
    { "TO", "Tonga" },
    { "TR", "Turkey" },
    { "TT", "Trinidad and Tobago" },
    { "TV", "Tuvalu" },
    { "TW", "Taiwan" },
    { "TZ", "Tanzania" },
    { "UA", "Ukraine" },
    { "UG", "Uganda" },
    { "UM", "U.S. Outlying Islands" },
    { "US", "United States of America" },
    { "UY", "Uruguay" },
    { "UZ", "Uzbekistan" },
    { "VA", "Holy See" },
    { "VC", "St. Vincent and the Grenadines" },
    { "VE", "Venezuela" },
    { "VG", "British Virgin Islands" },
    { "VI", "U.S. Virgin Islands" },
    { "VN", "Viet Nam" },
    { "VU", "Vanuatu" },
    { "WF", "Wallis and Futuna" },
    { "WS", "Samoa" },
    { "XK", "Kosovo" }, // Temporary code, no official ISO code assigned.
    { "YE", "Yemen" },
    { "YT", "Mayotte" },
    { "ZA", "South Africa" },
    { "ZM", "Zambia" },
    { "ZW", "Zimbabwe" },
    { nullptr, nullptr }
};

// Languages code, language name.
// Reference:
// - https://en.wikipedia.org/wiki/List_of_ISO_639-1_codes
// - https://www.localeplanet.com/icu/index.html
// - https://lh.2xlibre.net/locales/

static const char *language_list[][2] = {
    { "aa", "Afar" },
    { "ab", "Abkhazian" },
    { "ace", "Achinese" },
    { "ach", "Acoli" },
    { "ada", "Adangme" },
    { "ady", "Adyghe" },
    { "ae", "Avestan" },
    { "aeb", "Tunisian Arabic" },
    { "af", "Afrikaans" },
    { "afh", "Afrihili" },
    { "agq", "Aghem" },
    { "ain", "Ainu" },
    { "agr", "Aguaruna" },
    { "ak", "Akan" },
    { "akk", "Akkadian" },
    { "akz", "Alabama" },
    { "ale", "Aleut" },
    { "aln", "Gheg Albanian" },
    { "alt", "Southern Altai" },
    { "am", "Amharic" },
    { "an", "Aragonese" },
    { "ang", "Old English" },
    { "anp", "Angika" },
    { "ar", "Arabic" },
    { "arc", "Aramaic" },
    { "arn", "Mapudungun" },
    { "aro", "Araona" },
    { "arp", "Arapaho" },
    { "arq", "Algerian Arabic" },
    { "ars", "Najdi Arabic" },
    { "arw", "Arawak" },
    { "ary", "Moroccan Arabic" },
    { "arz", "Egyptian Arabic" },
    { "as", "Assamese" },
    { "asa", "Asu" },
    { "ase", "American Sign Language" },
    { "ast", "Asturian" },
    { "av", "Avaric" },
    { "avk", "Kotava" },
    { "awa", "Awadhi" },
    { "ayc", "Southern Aymara" },
    { "ay", "Aymara" },
    { "az", "Azerbaijani" },
    { "ba", "Bashkir" },
    { "bal", "Baluchi" },
    { "ban", "Balinese" },
    { "bar", "Bavarian" },
    { "bas", "Bassa" },
    { "bax", "Bamun" },
    { "bbc", "Batak Toba" },
    { "bbj", "Ghomala" },
    { "be", "Belarusian" },
    { "bej", "Beja" },
    { "bem", "Bemba" },
    { "ber", "Berber" },
    { "bew", "Betawi" },
    { "bez", "Bena" },
    { "bfd", "Bafut" },
    { "bfq", "Badaga" },
    { "bg", "Bulgarian" },
    { "bhb", "Bhili" },
    { "bgn", "Western Balochi" },
    { "bho", "Bhojpuri" },
    { "bi", "Bislama" },
    { "bik", "Bikol" },
    { "bin", "Bini" },
    { "bjn", "Banjar" },
    { "bkm", "Kom" },
    { "bla", "Siksika" },
    { "bm", "Bambara" },
    { "bn", "Bengali" },
    { "bo", "Tibetan" },
    { "bpy", "Bishnupriya" },
    { "bqi", "Bakhtiari" },
    { "br", "Breton" },
    { "brh", "Brahui" },
    { "brx", "Bodo" },
    { "bs", "Bosnian" },
    { "bss", "Akoose" },
    { "bua", "Buriat" },
    { "bug", "Buginese" },
    { "bum", "Bulu" },
    { "byn", "Bilin" },
    { "byv", "Medumba" },
    { "ca", "Catalan" },
    { "cad", "Caddo" },
    { "car", "Carib" },
    { "cay", "Cayuga" },
    { "cch", "Atsam" },
    { "ccp", "Chakma" },
    { "ce", "Chechen" },
    { "ceb", "Cebuano" },
    { "cgg", "Chiga" },
    { "ch", "Chamorro" },
    { "chb", "Chibcha" },
    { "chg", "Chagatai" },
    { "chk", "Chuukese" },
    { "chm", "Mari" },
    { "chn", "Chinook Jargon" },
    { "cho", "Choctaw" },
    { "chp", "Chipewyan" },
    { "chr", "Cherokee" },
    { "chy", "Cheyenne" },
    { "cic", "Chickasaw" },
    { "ckb", "Central Kurdish" },
    { "csb", "Kashubian" },
    { "cmn", "Mandarin Chinese" },
    { "co", "Corsican" },
    { "cop", "Coptic" },
    { "cps", "Capiznon" },
    { "cr", "Cree" },
    { "crh", "Crimean Tatar" },
    { "crs", "Seselwa Creole French" },
    { "cs", "Czech" },
    { "csb", "Kashubian" },
    { "cu", "Church Slavic" },
    { "cv", "Chuvash" },
    { "cy", "Welsh" },
    { "da", "Danish" },
    { "dak", "Dakota" },
    { "dar", "Dargwa" },
    { "dav", "Taita" },
    { "de", "German" },
    { "del", "Delaware" },
    { "den", "Slave" },
    { "dgr", "Dogrib" },
    { "din", "Dinka" },
    { "dje", "Zarma" },
    { "doi", "Dogri" },
    { "dsb", "Lower Sorbian" },
    { "dtp", "Central Dusun" },
    { "dua", "Duala" },
    { "dum", "Middle Dutch" },
    { "dv", "Dhivehi" },
    { "dyo", "Jola-Fonyi" },
    { "dyu", "Dyula" },
    { "dz", "Dzongkha" },
    { "dzg", "Dazaga" },
    { "ebu", "Embu" },
    { "ee", "Ewe" },
    { "efi", "Efik" },
    { "egl", "Emilian" },
    { "egy", "Ancient Egyptian" },
    { "eka", "Ekajuk" },
    { "el", "Greek" },
    { "elx", "Elamite" },
    { "en", "English" },
    { "enm", "Middle English" },
    { "eo", "Esperanto" },
    { "es", "Spanish" },
    { "esu", "Central Yupik" },
    { "et", "Estonian" },
    { "eu", "Basque" },
    { "ewo", "Ewondo" },
    { "ext", "Extremaduran" },
    { "fa", "Persian" },
    { "fan", "Fang" },
    { "fat", "Fanti" },
    { "ff", "Fulah" },
    { "fi", "Finnish" },
    { "fil", "Filipino" },
    { "fit", "Tornedalen Finnish" },
    { "fj", "Fijian" },
    { "fo", "Faroese" },
    { "fon", "Fon" },
    { "fr", "French" },
    { "frc", "Cajun French" },
    { "frm", "Middle French" },
    { "fro", "Old French" },
    { "frp", "Arpitan" },
    { "frr", "Northern Frisian" },
    { "frs", "Eastern Frisian" },
    { "fur", "Friulian" },
    { "fy", "Western Frisian" },
    { "ga", "Irish" },
    { "gaa", "Ga" },
    { "gag", "Gagauz" },
    { "gan", "Gan Chinese" },
    { "gay", "Gayo" },
    { "gba", "Gbaya" },
    { "gbz", "Zoroastrian Dari" },
    { "gd", "Scottish Gaelic" },
    { "gez", "Geez" },
    { "gil", "Gilbertese" },
    { "gl", "Galician" },
    { "glk", "Gilaki" },
    { "gmh", "Middle High German" },
    { "gn", "Guarani" },
    { "goh", "Old High German" },
    { "gom", "Goan Konkani" },
    { "gon", "Gondi" },
    { "gor", "Gorontalo" },
    { "got", "Gothic" },
    { "grb", "Grebo" },
    { "grc", "Ancient Greek" },
    { "gsw", "Swiss German" },
    { "gu", "Gujarati" },
    { "guc", "Wayuu" },
    { "gur", "Frafra" },
    { "guz", "Gusii" },
    { "gv", "Manx" },
    { "gwi", "Gwichʼin" },
    { "ha", "Hausa" },
    { "hai", "Haida" },
    { "hak", "Hakka Chinese" },
    { "haw", "Hawaiian" },
    { "he", "Hebrew" },
    { "hi", "Hindi" },
    { "hif", "Fiji Hindi" },
    { "hil", "Hiligaynon" },
    { "hit", "Hittite" },
    { "hmn", "Hmong" },
    { "ho", "Hiri Motu" },
    { "hne", "Chhattisgarhi" },
    { "hr", "Croatian" },
    { "hsb", "Upper Sorbian" },
    { "hsn", "Xiang Chinese" },
    { "ht", "Haitian" },
    { "hu", "Hungarian" },
    { "hup", "Hupa" },
    { "hus", "Huastec" },
    { "hy", "Armenian" },
    { "hz", "Herero" },
    { "ia", "Interlingua" },
    { "iba", "Iban" },
    { "ibb", "Ibibio" },
    { "id", "Indonesian" },
    { "ie", "Interlingue" },
    { "ig", "Igbo" },
    { "ii", "Sichuan Yi" },
    { "ik", "Inupiaq" },
    { "ilo", "Iloko" },
    { "inh", "Ingush" },
    { "io", "Ido" },
    { "is", "Icelandic" },
    { "it", "Italian" },
    { "iu", "Inuktitut" },
    { "izh", "Ingrian" },
    { "ja", "Japanese" },
    { "jam", "Jamaican Creole English" },
    { "jbo", "Lojban" },
    { "jgo", "Ngomba" },
    { "jmc", "Machame" },
    { "jpr", "Judeo-Persian" },
    { "jrb", "Judeo-Arabic" },
    { "jut", "Jutish" },
    { "jv", "Javanese" },
    { "ka", "Georgian" },
    { "kaa", "Kara-Kalpak" },
    { "kab", "Kabyle" },
    { "kac", "Kachin" },
    { "kaj", "Jju" },
    { "kam", "Kamba" },
    { "kaw", "Kawi" },
    { "kbd", "Kabardian" },
    { "kbl", "Kanembu" },
    { "kcg", "Tyap" },
    { "kde", "Makonde" },
    { "kea", "Kabuverdianu" },
    { "ken", "Kenyang" },
    { "kfo", "Koro" },
    { "kg", "Kongo" },
    { "kgp", "Kaingang" },
    { "kha", "Khasi" },
    { "kho", "Khotanese" },
    { "khq", "Koyra Chiini" },
    { "khw", "Khowar" },
    { "ki", "Kikuyu" },
    { "kiu", "Kirmanjki" },
    { "kj", "Kuanyama" },
    { "kk", "Kazakh" },
    { "kkj", "Kako" },
    { "kl", "Kalaallisut" },
    { "kln", "Kalenjin" },
    { "km", "Central Khmer" },
    { "kmb", "Kimbundu" },
    { "kn", "Kannada" },
    { "ko", "Korean" },
    { "koi", "Komi-Permyak" },
    { "kok", "Konkani" },
    { "kos", "Kosraean" },
    { "kpe", "Kpelle" },
    { "kr", "Kanuri" },
    { "krc", "Karachay-Balkar" },
    { "kri", "Krio" },
    { "krj", "Kinaray-a" },
    { "krl", "Karelian" },
    { "kru", "Kurukh" },
    { "ks", "Kashmiri" },
    { "ksb", "Shambala" },
    { "ksf", "Bafia" },
    { "ksh", "Colognian" },
    { "ku", "Kurdish" },
    { "kum", "Kumyk" },
    { "kut", "Kutenai" },
    { "kv", "Komi" },
    { "kw", "Cornish" },
    { "ky", "Kirghiz" },
    { "lag", "Langi" },
    { "la", "Latin" },
    { "lad", "Ladino" },
    { "lag", "Langi" },
    { "lah", "Lahnda" },
    { "lam", "Lamba" },
    { "lb", "Luxembourgish" },
    { "lez", "Lezghian" },
    { "lfn", "Lingua Franca Nova" },
    { "lg", "Ganda" },
    { "li", "Limburgan" },
    { "lij", "Ligurian" },
    { "liv", "Livonian" },
    { "lkt", "Lakota" },
    { "lmo", "Lombard" },
    { "ln", "Lingala" },
    { "lo", "Lao" },
    { "lol", "Mongo" },
    { "lou", "Louisiana Creole" },
    { "loz", "Lozi" },
    { "lrc", "Northern Luri" },
    { "lt", "Lithuanian" },
    { "ltg", "Latgalian" },
    { "lu", "Luba-Katanga" },
    { "lua", "Luba-Lulua" },
    { "lui", "Luiseno" },
    { "lun", "Lunda" },
    { "luo", "Luo" },
    { "lus", "Mizo" },
    { "luy", "Luyia" },
    { "lv", "Latvian" },
    { "lzh", "Literary Chinese" },
    { "lzz", "Laz" },
    { "mad", "Madurese" },
    { "maf", "Mafa" },
    { "mag", "Magahi" },
    { "mai", "Maithili" },
    { "mak", "Makasar" },
    { "man", "Mandingo" },
    { "mas", "Masai" },
    { "mde", "Maba" },
    { "mdf", "Moksha" },
    { "mdr", "Mandar" },
    { "men", "Mende" },
    { "mer", "Meru" },
    { "mfe", "Morisyen" },
    { "mg", "Malagasy" },
    { "mga", "Middle Irish" },
    { "mgh", "Makhuwa-Meetto" },
    { "mgo", "Metaʼ" },
    { "mh", "Marshallese" },
    { "mhr", "Eastern Mari" },
    { "mi", "Māori" },
    { "mic", "Mi'kmaq" },
    { "min", "Minangkabau" },
    { "miq", "Mískito" },
    { "mjw", "Karbi" },
    { "mk", "Macedonian" },
    { "ml", "Malayalam" },
    { "mn", "Mongolian" },
    { "mnc", "Manchu" },
    { "mni", "Manipuri" },
    { "mnw", "Mon" },
    { "mos", "Mossi" },
    { "moh", "Mohawk" },
    { "mr", "Marathi" },
    { "mrj", "Western Mari" },
    { "ms", "Malay" },
    { "mt", "Maltese" },
    { "mua", "Mundang" },
    { "mus", "Muscogee" },
    { "mwl", "Mirandese" },
    { "mwr", "Marwari" },
    { "mwv", "Mentawai" },
    { "my", "Burmese" },
    { "mye", "Myene" },
    { "myv", "Erzya" },
    { "mzn", "Mazanderani" },
    { "na", "Nauru" },
    { "nah", "Nahuatl" },
    { "nan", "Min Nan Chinese" },
    { "nap", "Neapolitan" },
    { "naq", "Nama" },
    { "nan", "Min Nan Chinese" },
    { "nb", "Norwegian Bokmål" },
    { "nd", "North Ndebele" },
    { "nds", "Low German" },
    { "ne", "Nepali" },
    { "new", "Newari" },
    { "nhn", "Central Nahuatl" },
    { "ng", "Ndonga" },
    { "nia", "Nias" },
    { "niu", "Niuean" },
    { "njo", "Ao Naga" },
    { "nl", "Dutch" },
    { "nmg", "Kwasio" },
    { "nn", "Norwegian Nynorsk" },
    { "nnh", "Ngiemboon" },
    { "nog", "Nogai" },
    { "non", "Old Norse" },
    { "nov", "Novial" },
    { "nqo", "N'ko" },
    { "nr", "South Ndebele" },
    { "nso", "Pedi" },
    { "nus", "Nuer" },
    { "nv", "Navajo" },
    { "nwc", "Classical Newari" },
    { "ny", "Nyanja" },
    { "nym", "Nyamwezi" },
    { "nyn", "Nyankole" },
    { "nyo", "Nyoro" },
    { "nzi", "Nzima" },
    { "oc", "Occitan" },
    { "oj", "Ojibwa" },
    { "om", "Oromo" },
    { "or", "Odia" },
    { "os", "Ossetic" },
    { "osa", "Osage" },
    { "ota", "Ottoman Turkish" },
    { "pa", "Panjabi" },
    { "pag", "Pangasinan" },
    { "pal", "Pahlavi" },
    { "pam", "Pampanga" },
    { "pap", "Papiamento" },
    { "pau", "Palauan" },
    { "pcd", "Picard" },
    { "pcm", "Nigerian Pidgin" },
    { "pdc", "Pennsylvania German" },
    { "pdt", "Plautdietsch" },
    { "peo", "Old Persian" },
    { "pfl", "Palatine German" },
    { "phn", "Phoenician" },
    { "pi", "Pali" },
    { "pl", "Polish" },
    { "pms", "Piedmontese" },
    { "pnt", "Pontic" },
    { "pon", "Pohnpeian" },
    { "pr", "Pirate" },
    { "prg", "Prussian" },
    { "pro", "Old Provençal" },
    { "prs", "Dari" },
    { "ps", "Pushto" },
    { "pt", "Portuguese" },
    { "qu", "Quechua" },
    { "quc", "K'iche" },
    { "qug", "Chimborazo Highland Quichua" },
    { "quy", "Ayacucho Quechua" },
    { "quz", "Cusco Quechua" },
    { "raj", "Rajasthani" },
    { "rap", "Rapanui" },
    { "rar", "Rarotongan" },
    { "rgn", "Romagnol" },
    { "rif", "Riffian" },
    { "rm", "Romansh" },
    { "rn", "Rundi" },
    { "ro", "Romanian" },
    { "rof", "Rombo" },
    { "rom", "Romany" },
    { "rtm", "Rotuman" },
    { "ru", "Russian" },
    { "rue", "Rusyn" },
    { "rug", "Roviana" },
    { "rup", "Aromanian" },
    { "rw", "Kinyarwanda" },
    { "rwk", "Rwa" },
    { "sa", "Sanskrit" },
    { "sad", "Sandawe" },
    { "sah", "Sakha" },
    { "sam", "Samaritan Aramaic" },
    { "saq", "Samburu" },
    { "sas", "Sasak" },
    { "sat", "Santali" },
    { "saz", "Saurashtra" },
    { "sba", "Ngambay" },
    { "sbp", "Sangu" },
    { "sc", "Sardinian" },
    { "scn", "Sicilian" },
    { "sco", "Scots" },
    { "sd", "Sindhi" },
    { "sdc", "Sassarese Sardinian" },
    { "sdh", "Southern Kurdish" },
    { "se", "Northern Sami" },
    { "see", "Seneca" },
    { "seh", "Sena" },
    { "sei", "Seri" },
    { "sel", "Selkup" },
    { "ses", "Koyraboro Senni" },
    { "sg", "Sango" },
    { "sga", "Old Irish" },
    { "sgs", "Samogitian" },
    { "sh", "Serbo-Croatian" },
    { "shi", "Tachelhit" },
    { "shn", "Shan" },
    { "shs", "Shuswap" },
    { "shu", "Chadian Arabic" },
    { "si", "Sinhala" },
    { "sid", "Sidamo" },
    { "sk", "Slovak" },
    { "sl", "Slovenian" },
    { "sli", "Lower Silesian" },
    { "sly", "Selayar" },
    { "sm", "Samoan" },
    { "sma", "Southern Sami" },
    { "smj", "Lule Sami" },
    { "smn", "Inari Sami" },
    { "sms", "Skolt Sami" },
    { "sn", "Shona" },
    { "snk", "Soninke" },
    { "so", "Somali" },
    { "sog", "Sogdien" },
    { "son", "Songhai" },
    { "sq", "Albanian" },
    { "sr", "Serbian" },
    { "srn", "Sranan Tongo" },
    { "srr", "Serer" },
    { "ss", "Swati" },
    { "ssy", "Saho" },
    { "st", "Southern Sotho" },
    { "stq", "Saterland Frisian" },
    { "su", "Sundanese" },
    { "suk", "Sukuma" },
    { "sus", "Susu" },
    { "sux", "Sumerian" },
    { "sv", "Swedish" },
    { "sw", "Swahili" },
    { "swb", "Comorian" },
    { "swc", "Congo Swahili" },
    { "syc", "Classical Syriac" },
    { "syr", "Syriac" },
    { "szl", "Silesian" },
    { "ta", "Tamil" },
    { "tcy", "Tulu" },
    { "te", "Telugu" },
    { "tem", "Timne" },
    { "teo", "Teso" },
    { "ter", "Tereno" },
    { "tet", "Tetum" },
    { "tg", "Tajik" },
    { "th", "Thai" },
    { "the", "Chitwania Tharu" },
    { "ti", "Tigrinya" },
    { "tig", "Tigre" },
    { "tiv", "Tiv" },
    { "tk", "Turkmen" },
    { "tkl", "Tokelau" },
    { "tkr", "Tsakhur" },
    { "tl", "Tagalog" },
    { "tlh", "Klingon" },
    { "tli", "Tlingit" },
    { "tly", "Talysh" },
    { "tmh", "Tamashek" },
    { "tn", "Tswana" },
    { "to", "Tongan" },
    { "tog", "Nyasa Tonga" },
    { "tpi", "Tok Pisin" },
    { "tr", "Turkish" },
    { "tru", "Turoyo" },
    { "trv", "Taroko" },
    { "ts", "Tsonga" },
    { "tsd", "Tsakonian" },
    { "tsi", "Tsimshian" },
    { "tt", "Tatar" },
    { "ttt", "Muslim Tat" },
    { "tum", "Tumbuka" },
    { "tvl", "Tuvalu" },
    { "tw", "Twi" },
    { "twq", "Tasawaq" },
    { "ty", "Tahitian" },
    { "tyv", "Tuvinian" },
    { "tzm", "Central Atlas Tamazight" },
    { "udm", "Udmurt" },
    { "ug", "Uyghur" },
    { "uga", "Ugaritic" },
    { "uk", "Ukrainian" },
    { "umb", "Umbundu" },
    { "unm", "Unami" },
    { "ur", "Urdu" },
    { "uz", "Uzbek" },
    { "vai", "Vai" },
    { "ve", "Venda" },
    { "vec", "Venetian" },
    { "vep", "Veps" },
    { "vi", "Vietnamese" },
    { "vls", "West Flemish" },
    { "vmf", "Main-Franconian" },
    { "vo", "Volapük" },
    { "vot", "Votic" },
    { "vro", "Võro" },
    { "vun", "Vunjo" },
    { "wa", "Walloon" },
    { "wae", "Walser" },
    { "wal", "Wolaytta" },
    { "war", "Waray" },
    { "was", "Washo" },
    { "wbp", "Warlpiri" },
    { "wo", "Wolof" },
    { "wuu", "Wu Chinese" },
    { "xal", "Kalmyk" },
    { "xh", "Xhosa" },
    { "xmf", "Mingrelian" },
    { "xog", "Soga" },
    { "yao", "Yao" },
    { "yap", "Yapese" },
    { "yav", "Yangben" },
    { "ybb", "Yemba" },
    { "yi", "Yiddish" },
    { "yo", "Yoruba" },
    { "yrl", "Nheengatu" },
    { "yue", "Yue Chinese" },
    { "yuw", "Papua New Guinea" },
    { "za", "Zhuang" },
    { "zap", "Zapotec" },
    { "zbl", "Blissymbols" },
    { "zea", "Zeelandic" },
    { "zen", "Zenaga" },
    { "zgh", "Standard Moroccan Tamazight" },
    { "zh", "Chinese" },
    { "zu", "Zulu" },
    { "zun", "Zuni" },
    { "zza", "Zaza" },
    { nullptr, nullptr }
};

// Additional regional variants.
// Variant name, supported languages.

static const char *locale_variants[][2] = {
    { "valencia", "ca" },
    { "iqtelif", "tt" },
    { "saaho", "aa" },
    { "tradnl", "es" },
    { nullptr, nullptr },
};

// Script names and codes (excludes typographic variants, special codes, reserved codes and aliases for combined scripts).
// Reference:
// - https://en.wikipedia.org/wiki/ISO_15924

static const char *script_list[][2] = {
    { "Adlam", "Adlm" },
    { "Afaka", "Afak" },
    { "Caucasian Albanian", "Aghb" },
    { "Ahom", "Ahom" },
    { "Arabic", "Arab" },
    { "Imperial Aramaic", "Armi" },
    { "Armenian", "Armn" },
    { "Avestan", "Avst" },
    { "Balinese", "Bali" },
    { "Bamum", "Bamu" },
    { "Bassa Vah", "Bass" },
    { "Batak", "Batk" },
    { "Bengali", "Beng" },
    { "Bhaiksuki", "Bhks" },
    { "Blissymbols", "Blis" },
    { "Bopomofo", "Bopo" },
    { "Brahmi", "Brah" },
    { "Braille", "Brai" },
    { "Buginese", "Bugi" },
    { "Buhid", "Buhd" },
    { "Chakma", "Cakm" },
    { "Unified Canadian Aboriginal", "Cans" },
    { "Carian", "Cari" },
    { "Cham", "Cham" },
    { "Cherokee", "Cher" },
    { "Chorasmian", "Chrs" },
    { "Cirth", "Cirt" },
    { "Coptic", "Copt" },
    { "Cypro-Minoan", "Cpmn" },
    { "Cypriot", "Cprt" },
    { "Cyrillic", "Cyrl" },
    { "Devanagari", "Deva" },
    { "Dives Akuru", "Diak" },
    { "Dogra", "Dogr" },
    { "Deseret", "Dsrt" },
    { "Duployan", "Dupl" },
    { "Egyptian demotic", "Egyd" },
    { "Egyptian hieratic", "Egyh" },
    { "Egyptian hieroglyphs", "Egyp" },
    { "Elbasan", "Elba" },
    { "Elymaic", "Elym" },
    { "Ethiopic", "Ethi" },
    { "Khutsuri", "Geok" },
    { "Georgian", "Geor" },
    { "Glagolitic", "Glag" },
    { "Gunjala Gondi", "Gong" },
    { "Masaram Gondi", "Gonm" },
    { "Gothic", "Goth" },
    { "Grantha", "Gran" },
    { "Greek", "Grek" },
    { "Gujarati", "Gujr" },
    { "Gurmukhi", "Guru" },
    { "Hangul", "Hang" },
    { "Han", "Hani" },
    { "Hanunoo", "Hano" },
    { "Simplified", "Hans" },
    { "Traditional", "Hant" },
    { "Hatran", "Hatr" },
    { "Hebrew", "Hebr" },
    { "Hiragana", "Hira" },
    { "Anatolian Hieroglyphs", "Hluw" },
    { "Pahawh Hmong", "Hmng" },
    { "Nyiakeng Puachue Hmong", "Hmnp" },
    { "Old Hungarian", "Hung" },
    { "Indus", "Inds" },
    { "Old Italic", "Ital" },
    { "Javanese", "Java" },
    { "Jurchen", "Jurc" },
    { "Kayah Li", "Kali" },
    { "Katakana", "Kana" },
    { "Kharoshthi", "Khar" },
    { "Khmer", "Khmr" },
    { "Khojki", "Khoj" },
    { "Khitan large script", "Kitl" },
    { "Khitan small script", "Kits" },
    { "Kannada", "Knda" },
    { "Kpelle", "Kpel" },
    { "Kaithi", "Kthi" },
    { "Tai Tham", "Lana" },
    { "Lao", "Laoo" },
    { "Latin", "Latn" },
    { "Leke", "Leke" },
    { "Lepcha", "Lepc" },
    { "Limbu", "Limb" },
    { "Linear A", "Lina" },
    { "Linear B", "Linb" },
    { "Lisu", "Lisu" },
    { "Loma", "Loma" },
    { "Lycian", "Lyci" },
    { "Lydian", "Lydi" },
    { "Mahajani", "Mahj" },
    { "Makasar", "Maka" },
    { "Mandaic", "Mand" },
    { "Manichaean", "Mani" },
    { "Marchen", "Marc" },
    { "Mayan Hieroglyphs", "Maya" },
    { "Medefaidrin", "Medf" },
    { "Mende Kikakui", "Mend" },
    { "Meroitic Cursive", "Merc" },
    { "Meroitic Hieroglyphs", "Mero" },
    { "Malayalam", "Mlym" },
    { "Modi", "Modi" },
    { "Mongolian", "Mong" },
    { "Moon", "Moon" },
    { "Mro", "Mroo" },
    { "Meitei Mayek", "Mtei" },
    { "Multani", "Mult" },
    { "Myanmar (Burmese)", "Mymr" },
    { "Nandinagari", "Nand" },
    { "Old North Arabian", "Narb" },
    { "Nabataean", "Nbat" },
    { "Newa", "Newa" },
    { "Naxi Dongba", "Nkdb" },
    { "Nakhi Geba", "Nkgb" },
    { "N'ko", "Nkoo" },
    { "Nüshu", "Nshu" },
    { "Ogham", "Ogam" },
    { "Ol Chiki", "Olck" },
    { "Old Turkic", "Orkh" },
    { "Oriya", "Orya" },
    { "Osage", "Osge" },
    { "Osmanya", "Osma" },
    { "Old Uyghur", "Ougr" },
    { "Palmyrene", "Palm" },
    { "Pau Cin Hau", "Pauc" },
    { "Proto-Cuneiform", "Pcun" },
    { "Proto-Elamite", "Pelm" },
    { "Old Permic", "Perm" },
    { "Phags-pa", "Phag" },
    { "Inscriptional Pahlavi", "Phli" },
    { "Psalter Pahlavi", "Phlp" },
    { "Book Pahlavi", "Phlv" },
    { "Phoenician", "Phnx" },
    { "Klingon", "Piqd" },
    { "Miao", "Plrd" },
    { "Inscriptional Parthian", "Prti" },
    { "Proto-Sinaitic", "Psin" },
    { "Ranjana", "Ranj" },
    { "Rejang", "Rjng" },
    { "Hanifi Rohingya", "Rohg" },
    { "Rongorongo", "Roro" },
    { "Runic", "Runr" },
    { "Samaritan", "Samr" },
    { "Sarati", "Sara" },
    { "Old South Arabian", "Sarb" },
    { "Saurashtra", "Saur" },
    { "SignWriting", "Sgnw" },
    { "Shavian", "Shaw" },
    { "Sharada", "Shrd" },
    { "Shuishu", "Shui" },
    { "Siddham", "Sidd" },
    { "Khudawadi", "Sind" },
    { "Sinhala", "Sinh" },
    { "Sogdian", "Sogd" },
    { "Old Sogdian", "Sogo" },
    { "Sora Sompeng", "Sora" },
    { "Soyombo", "Soyo" },
    { "Sundanese", "Sund" },
    { "Syloti Nagri", "Sylo" },
    { "Syriac", "Syrc" },
    { "Tagbanwa", "Tagb" },
    { "Takri", "Takr" },
    { "Tai Le", "Tale" },
    { "New Tai Lue", "Talu" },
    { "Tamil", "Taml" },
    { "Tangut", "Tang" },
    { "Tai Viet", "Tavt" },
    { "Telugu", "Telu" },
    { "Tengwar", "Teng" },
    { "Tifinagh", "Tfng" },
    { "Tagalog", "Tglg" },
    { "Thaana", "Thaa" },
    { "Thai", "Thai" },
    { "Tibetan", "Tibt" },
    { "Tirhuta", "Tirh" },
    { "Tangsa", "Tnsa" },
    { "Toto", "Toto" },
    { "Ugaritic", "Ugar" },
    { "Vai", "Vaii" },
    { "Visible Speech", "Visp" },
    { "Vithkuqi", "Vith" },
    { "Warang Citi", "Wara" },
    { "Wancho", "Wcho" },
    { "Woleai", "Wole" },
    { "Old Persian", "Xpeo" },
    { "Cuneiform", "Xsux" },
    { "Yezidi", "Yezi" },
    { "Yi", "Yiii" },
    { "Zanabazar Square", "Zanb" },
    { nullptr, nullptr }
};

///////////////////////////////////////////////

PoolStringArray Translation::_get_messages() const {

    PoolStringArray msgs;
    msgs.resize(translation_map.size() * 2);
    int idx = 0;
    for (const eastl::pair<const StringName,StringName> &E : translation_map) {

        msgs.set(idx + 0, String(E.first));
        msgs.set(idx + 1, String(E.second));
        idx += 2;
    }

    return msgs;
}

PoolStringArray Translation::_get_message_list() const {

    PoolStringArray msgs;
    msgs.resize(translation_map.size());
    int idx = 0;
    for (const eastl::pair<const StringName,StringName> &E : translation_map) {

        msgs.set(idx, String(E.first));
        idx += 1;
    }

    return msgs;
}

void Translation::_set_messages(const PoolVector<String> &p_messages) {

    int msg_count = p_messages.size();
    ERR_FAIL_COND(msg_count % 2);

    PoolVector<String>::Read r = p_messages.read();

    for (int i = 0; i < msg_count; i += 2) {

        add_message(StringName(r[i + 0]), StringName(r[i + 1]));
    }
}

void Translation::set_locale(StringView p_locale) {
    locale = TranslationServer::get_singleton()->standardize_locale(p_locale);

    if (OS::get_singleton()->get_main_loop()) {
        OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_TRANSLATION_CHANGED);
    }
}

void Translation::add_context_message(
        const StringName &p_src_text, const StringName &p_xlated_text, const StringName &p_context) {
    if (p_context != StringName()) {
        WARN_PRINT("Translation class doesn't handle context.");
    }
    add_message(p_src_text, p_xlated_text);
}

StringName Translation::get_context_message(const StringName &p_src_text, const StringName &p_context) const {
    if (p_context != StringName()) {
        WARN_PRINT("Translation class doesn't handle context.");
    }
    return get_message(p_src_text);
}
void Translation::add_message(const StringName &p_src_text, const StringName &p_xlated_text) {

    translation_map[p_src_text] = p_xlated_text;
}
StringName Translation::get_message(const StringName &p_src_text) const {
    if (get_script_instance()) {
        return get_script_instance()->call("_get_message", p_src_text).as<StringName>();
    }

    auto E = translation_map.find(p_src_text);
    if (E==translation_map.end())
        return StringName();

    return E->second;
}

void Translation::erase_message(const StringName &p_src_text) {

    translation_map.erase(p_src_text);
}

void Translation::get_message_list(List<StringName> *r_messages) const {

    for (const eastl::pair<const StringName,StringName> &E : translation_map) {
        r_messages->emplace_back(E.first);
    }
}

int Translation::get_message_count() const {

    return translation_map.size();
}

void Translation::_bind_methods() {

    SE_BIND_METHOD(Translation,set_locale);
    SE_BIND_METHOD(Translation,get_locale);
    SE_BIND_METHOD(Translation,add_message);
    SE_BIND_METHOD(Translation,get_message);
    SE_BIND_METHOD(Translation,erase_message);
    MethodBinder::bind_method(D_METHOD("get_message_list"), &Translation::_get_message_list);
    SE_BIND_METHOD(Translation,get_message_count);
    SE_BIND_METHOD(Translation,_set_messages);
    SE_BIND_METHOD(Translation,_get_messages);

    BIND_VMETHOD(MethodInfo(VariantType::STRING, "_get_message", PropertyInfo(VariantType::STRING, "src_message")));
    ADD_PROPERTY(PropertyInfo(VariantType::POOL_STRING_ARRAY, "messages", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL), "_set_messages", "_get_messages");
    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "locale"), "set_locale", "get_locale");
}

Translation::Translation() :
        locale("en") {
}
///////////////////////////////////////////////

void ContextTranslation::add_context_message(
        const StringName &p_src_text, const StringName &p_xlated_text, const StringName &p_context) {
    if (p_context.empty()) {
        add_message(p_src_text, p_xlated_text);
    } else {
        context_translation_map[p_context][p_src_text] = p_xlated_text;
    }
}

StringName ContextTranslation::get_context_message(const StringName &p_src_text, const StringName &p_context) const {
    if (p_context == StringName()) {
        return get_message(p_src_text);
    }

    auto context_iter = context_translation_map.find(p_context);
    if (context_iter==context_translation_map.end()) {
        return StringName();
    }
    auto message_iter = context_iter->second.find(p_src_text);
    if (message_iter==context_iter->second.end()) {
        return StringName();
    }
    return message_iter->second;
}

///////////////////////////////////////////////

static _FORCE_INLINE_ bool is_ascii_upper_case(char32_t c) {
    return (c >= 'A' && c <= 'Z');
}

static _FORCE_INLINE_ bool is_ascii_lower_case(char32_t c) {
    return (c >= 'a' && c <= 'z');
}

Vector<TranslationServer::LocaleScriptInfo> TranslationServer::locale_script_info;

HashMap<String, String> TranslationServer::language_map;
HashMap<String, String> TranslationServer::script_map;
HashMap<String, String> TranslationServer::locale_rename_map;
HashMap<String, String> TranslationServer::country_name_map;
HashMap<String, String> TranslationServer::variant_map;
HashMap<String, String> TranslationServer::country_rename_map;

void TranslationServer::init_locale_info() {
    // Init locale info.
    language_map.clear();
    int idx = 0;
    while (language_list[idx][0] != nullptr) {
        language_map[language_list[idx][0]] = String(language_list[idx][1]);
        idx++;
    }

    // Init locale-script map.
    locale_script_info.clear();
    idx = 0;
    while (locale_scripts[idx][0] != nullptr) {
        LocaleScriptInfo info;
        info.name = locale_scripts[idx][0];
        info.script = locale_scripts[idx][1];
        info.default_country = locale_scripts[idx][2];
        Vector<StringView> supported_countries;
        String::split_ref(supported_countries,locale_scripts[idx][3], ",");
        for (int i = 0; i < supported_countries.size(); i++) {
            info.supported_countries.emplace(supported_countries[i]);
        }
        locale_script_info.push_back(info);
        idx++;
    }

    // Init supported script list.
    script_map.clear();
    idx = 0;
    while (script_list[idx][0] != nullptr) {
        script_map[script_list[idx][1]] = String(script_list[idx][0]);
        idx++;
    }

    // Init regional variant map.
    variant_map.clear();
    idx = 0;
    while (locale_variants[idx][0] != nullptr) {
        variant_map[locale_variants[idx][0]] = locale_variants[idx][1];
        idx++;
    }

    // Init locale renames.
    locale_rename_map.clear();
    idx = 0;
    while (locale_renames[idx][0] != nullptr) {
        if (!String(locale_renames[idx][1]).empty()) {
            locale_rename_map[locale_renames[idx][0]] = locale_renames[idx][1];
        }
        idx++;
    }

    // Init country names.
    country_name_map.clear();
    idx = 0;
    while (country_names[idx][0] != nullptr) {
        country_name_map[String(country_names[idx][0])] = String(country_names[idx][1]);
        idx++;
    }

    // Init country renames.
    country_rename_map.clear();
    idx = 0;
    while (country_renames[idx][0] != nullptr) {
        if (!String(country_renames[idx][1]).empty()) {
            country_rename_map[country_renames[idx][0]] = country_renames[idx][1];
        }
        idx++;
    }
}


String TranslationServer::standardize_locale(StringView p_locale) const {
    // Replaces '-' with '_' for macOS style locales.
    String univ_locale = String(p_locale).replaced("-", "_");

    // Extract locale elements.
    StringView script, country;

    String variant;
    Vector<StringView> locale_elements;
    String::split_ref(locale_elements, p_locale.substr(0, p_locale.find("@")), "-");
    StringView lang = locale_elements[0];

    if (locale_elements.size() >= 2) {
        if (locale_elements[1].length() == 4 && is_ascii_upper_case(locale_elements[1][0]) && is_ascii_lower_case(locale_elements[1][1]) &&
                is_ascii_lower_case(locale_elements[1][2]) && is_ascii_lower_case(locale_elements[1][3])) {
            script = locale_elements[1];
        }
        if (locale_elements[1].length() == 2 && is_ascii_upper_case(locale_elements[1][0]) && is_ascii_upper_case(locale_elements[1][1])) {
            country = locale_elements[1];
        }
    }
    if (locale_elements.size() >= 3) {
        if (locale_elements[2].length() == 2 && is_ascii_upper_case(locale_elements[2][0]) && is_ascii_upper_case(locale_elements[2][1])) {
            country = locale_elements[2];
        } else {
            String l2(locale_elements[2]);
            if (variant_map.contains(l2.to_lower()) && variant_map[l2.to_lower()] == lang) {
                variant = l2.to_lower();
            }
        }
    }
    if (locale_elements.size() >= 4) {
        String l3(locale_elements[3]);
        if (variant_map.contains(l3.to_lower()) && variant_map[l3.to_lower()] == lang) {
            variant = l3.to_lower();
        }
    }

    // Try extract script and variant from the extra part.
    StringView second_part(p_locale.substr(p_locale.find("@")+1));
    Vector<StringView> script_extra;
    String::split_ref(script_extra, second_part, ";");
    for (int i = 0; i < script_extra.size(); i++) {
        String extr(script_extra[i]);
        if (extr.to_lower() == "cyrillic") {
            script = "Cyrl";
            break;
        } else if (extr.to_lower() == "latin") {
            script = "Latn";
            break;
        } else if (extr.to_lower() == "devanagari") {
            script = "Deva";
            break;
        } else if (variant_map.contains(extr.to_lower()) && variant_map[extr.to_lower()] == lang) {
            variant = extr.to_lower();
        }
    }

    // Handles known non-ISO language names used e.g. on Windows.
    if (locale_rename_map.contains_as(lang)) {
        lang = locale_rename_map.at_as(lang);
    }

    // Handle country renames.
    if (country_rename_map.contains_as(country)) {
        country = country_rename_map.at_as(country);
    }

    // Remove unsupported script codes.
    if (!script_map.contains_as(script)) {
        script = "";
    }

    // Add script code base on language and country codes for some ambiguous cases.
    if (script.empty()) {
        for (int i = 0; i < locale_script_info.size(); i++) {
            const LocaleScriptInfo &info = locale_script_info[i];
            if (info.name == lang) {
                if (country.empty() || info.supported_countries.contains_as(country)) {
                    script = info.script;
                    break;
                }
            }
        }
    }
    if (!script.empty() && country.empty()) {
        // Add conntry code based on script for some ambiguous cases.
        for (int i = 0; i < locale_script_info.size(); i++) {
            const LocaleScriptInfo &info = locale_script_info[i];
            if (info.name == lang && info.script == script) {
                country = info.default_country;
                break;
            }
        }
    }

    // Combine results.
    String locale(lang);
    if (!script.empty()) {
        locale += "_" + script;
    }
    if (!country.empty()) {
        locale += "_" + country;
    }
    if (!variant.empty()) {
        locale += "_" + variant;
    }
    return locale;
}

int TranslationServer::compare_locales(StringView p_locale_a, StringView p_locale_b) const {
    String locale_a = standardize_locale(p_locale_a);
    String locale_b = standardize_locale(p_locale_b);

    if (locale_a == locale_b) {
        // Exact match.
        return 10;
    }

    FixedVector<StringView,5> locale_a_elements;
    FixedVector<StringView, 5> locale_b_elements;
    String::split_ref(locale_a_elements, locale_a, "_");
    String::split_ref(locale_b_elements, locale_b, "_");

    if (locale_a_elements[0] != locale_b_elements[0]) {
        // No match.
        return 0;
    }
    // Matching language, both locales have extra parts.
    // Return number of matching elements.
    int matching_elements = 1;
    for (int i = 1; i < locale_a_elements.size(); i++) {
        for (int j = 1; j < locale_b_elements.size(); j++) {
            if (locale_a_elements[i] == locale_b_elements[j]) {
                matching_elements++;
            }
        }
    }
    return matching_elements;
}

String TranslationServer::get_locale_name(StringView p_locale) const {
    String locale = standardize_locale(p_locale);

    String lang, script, country;
    FixedVector<StringView, 5> locale_elements;
    String::split_ref(locale_elements, locale, "_");

    lang = locale_elements[0];
    if (locale_elements.size() >= 2) {
        if (locale_elements[1].length() == 4 && is_ascii_upper_case(locale_elements[1][0]) && is_ascii_lower_case(locale_elements[1][1]) &&
                is_ascii_lower_case(locale_elements[1][2]) && is_ascii_lower_case(locale_elements[1][3])) {
            script = locale_elements[1];
        }
        if (locale_elements[1].length() == 2 && is_ascii_upper_case(locale_elements[1][0]) && is_ascii_upper_case(locale_elements[1][1])) {
            country = locale_elements[1];
        }
    }
    if (locale_elements.size() >= 3) {
        if (locale_elements[2].length() == 2 && is_ascii_upper_case(locale_elements[2][0]) && is_ascii_upper_case(locale_elements[2][1])) {
            country = locale_elements[2];
        }
    }

    String name = language_map[lang];
    if (!script.empty()) {
        name = name + " (" + script_map[script] + ")";
    }
    if (!country.empty()) {
        name = name + ", " + country_name_map[country];
    }
    return name;
}

Vector<String> TranslationServer::get_all_languages() const {
    return language_map.keys();
}

String TranslationServer::get_language_name(const String &p_language) const {
    return language_map[p_language];
}

Vector<String> TranslationServer::get_all_scripts() const {
    return script_map.keys();
}

String TranslationServer::get_script_name(const String &p_script) const {
    return script_map[p_script];
}

Vector<String> TranslationServer::get_all_countries() const {
    return country_name_map.keys();
}

String TranslationServer::get_country_name(const String &p_country) const {
    return country_name_map[p_country];
}

void TranslationServer::set_locale(StringView p_locale) {
    locale = standardize_locale(p_locale);

    if (OS::get_singleton()->get_main_loop()) {
        OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_TRANSLATION_CHANGED);
    }

    gResourceRemapper().reload_translation_remaps();
}

const String &TranslationServer::get_locale() const {
    return locale;
}

Array TranslationServer::get_loaded_locales() const {
    Array locales;
    for (const Ref<Translation> &t : translations) {
        ERR_FAIL_COND_V(!t, Array());
        String l = t->get_locale();

        locales.push_back(l);
    }

    return locales;
}

void TranslationServer::add_translation(const Ref<Translation> &p_translation) {

    translations.insert(p_translation);
}
void TranslationServer::remove_translation(const Ref<Translation> &p_translation) {

    translations.erase(p_translation);
}

void TranslationServer::clear() {

    translations.clear();
}

StringName TranslationServer::translate(StringView p_message) const {
    StringName default_return(p_message);
    // Match given message against the translation catalog for the project locale.

    if (!enabled) {
        return default_return;
    }

    StringName res;
    int best_score = 0;

    for (const Ref<Translation> &t : translations) {
        ERR_FAIL_COND_V(!t, default_return);
        String l = t->get_locale();

        int score = compare_locales(locale, l);
        if (score > 0 && score >= best_score) {
            StringName r = t->get_message(default_return);
            if (!r) {
                continue;
            }
            res = r;
            best_score = score;
            if (score == 10) {
                break; // Exact match, skip the rest.
            }
        }
    }

    if (!res && fallback.length() >= 2) {
        best_score = 0;

        for (const Ref<Translation> &t : translations) {
            ERR_FAIL_COND_V(!t, default_return);
            String l = t->get_locale();

            int score = compare_locales(fallback, l);
            if (score > 0 && score >= best_score) {
                StringName r = t->get_message(default_return);
                if (!r) {
                    continue;
                }
                res = r;
                best_score = score;
                if (score == 10) {
                    break; // Exact match, skip the rest.
                }
            }
        }
    }

    if (!res) {
        return default_return;
    }

    return res;
}

TranslationServer *TranslationServer::singleton = nullptr;

bool TranslationServer::_load_translations(const StringName &p_from) {

    if (ProjectSettings::get_singleton()->has_setting(p_from)) {
        auto translations(ProjectSettings::get_singleton()->get(p_from).as<PoolVector<String>>());

        int tcount = translations.size();

        if (tcount) {
            PoolVector<String>::Read r = translations.read();

            for (int i = 0; i < tcount; i++) {

                Ref<Translation> tr(dynamic_ref_cast<Translation>(gResourceManager().load(r[i])));
                if (tr)
                    add_translation(tr);
            }
        }
        return true;
    }

    return false;
}

void TranslationServer::set_tool_translation(const Ref<Translation> &p_translation) {
    tool_translation = p_translation;
}

StringName TranslationServer::tool_translate(const StringName &p_message, const StringName &p_context) const {

    if (tool_translation) {
        StringName r = tool_translation->get_context_message(p_message, p_context);

        if (r) {
            return r;
        }
    }

    return p_message;
}

void TranslationServer::set_doc_translation(const Ref<Translation> &p_translation) {
    doc_translation = p_translation;
}

StringName TranslationServer::doc_translate(const StringName &p_message) const {
    if (doc_translation) {
        StringName r = doc_translation->get_message(p_message);
        if (r) {
            return r;
        }
    }
    return p_message;
}

void TranslationServer::_bind_methods() {

    SE_BIND_METHOD(TranslationServer,set_locale);
    SE_BIND_METHOD(TranslationServer,get_locale);

    SE_BIND_METHOD(TranslationServer,get_locale_name);

    SE_BIND_METHOD(TranslationServer,translate);

    SE_BIND_METHOD(TranslationServer,add_translation);
    SE_BIND_METHOD(TranslationServer,remove_translation);

    SE_BIND_METHOD(TranslationServer,clear);

    SE_BIND_METHOD(TranslationServer,get_loaded_locales);
}

void TranslationServer::load_translations() {
    char buf[32] = {"locale/translations_"};
    String locale(get_locale());
    auto cnt = locale.size();
    _load_translations(buf); //all
    if(cnt>=2)
    {
        // generic locale
        buf[20]=locale[0];
        buf[21]=locale[1];
        _load_translations(buf);
    }
    if(cnt>2)
    {
        ERR_FAIL_COND(cnt+20>=sizeof(buf));
        if(cnt+20<sizeof(buf))
        {
            for(uint32_t i=0; i<cnt-2; ++i)
                buf[22+i] = locale[2+i];
            _load_translations(buf);
        }
    }
}

TranslationServer::TranslationServer() :
        locale("en"){
    singleton = this;

    init_locale_info();
}
