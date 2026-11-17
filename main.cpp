#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <cmath>
#include <sstream>
#include <cctype>
#include <stdexcept>
#include <locale>
#include <utility>

using namespace std;

constexpr double PI = 3.14159265358979323846;

struct Params {
    // Winkelbereich
    double phi_start_rad = 0.0;
    double phi_end_rad = 2.0 * PI;
    double dphi_rad = PI / 180.0;

    // Absolute Druecke [Pa]
    double p_int = 180000.0;
    double p_exh = 120000.0;
    double p0 = 100000.0;
    double p_max = 16000000.0;
    double p_tdc_base = 5000000.0;

    // Wiebe-Funktion
    double phi0_rad = -5.0 * PI / 180.0;
    double dphi_comb_rad = 45.0 * PI / 180.0;
    double a = 6.9;
    double m = 2.0;

    // Dateien und CSV
    string csv_filename = "pz_fm_phi.csv";
    string input_filename = "input.dat";
    bool write_deg_column = true;
    bool write_xb_column = true;
    char csv_sep = ';';

    // Geometrie und Betriebsdaten
    double Ak = 0.005153;  // Kolbenflaeche [m^2]
    double r = 0.04775;    // Kurbelradius [m]
    double l = 0.144;      // Pleuellaenge [m]
    double lambda = r / l;
    double omega = 200.0;  // Winkelgeschwindigkeit [rad/s]

    double mp = 0.6;       // Pleuelmasse [kg]
    double rp = 0.072;     // Schwerpunktlage Pleuel [m]
    double mpr = 0.3;      // Rotierende Pleuelmasse [kg]
    double mo = 1.062;     // Oszillierende Masse [kg]

    double mw = 1.6;       // Masse Kurbelwange [kg]
    double mz = 0.0;       // Zusaetzliche Zapfenmasse [kg]
    double rw = r;         // Wangenradius [m]
};

// Hilfsfunktionen

string trim(const string& s)
{
    size_t a = 0;
    size_t b = s.size();

    while (a < b && isspace(static_cast<unsigned char>(s[a]))) {
        ++a;
    }

    while (b > a && isspace(static_cast<unsigned char>(s[b - 1]))) {
        --b;
    }

    return s.substr(a, b - a);
}

void replace_all(string& s, const string& from, const string& to)
{
    if (from.empty()) {
        return;
    }

    size_t pos = 0;

    while ((pos = s.find(from, pos)) != string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
}

// Mini-Parser fuer Zahlen, pi und die Operatoren +, -, *, /.
// Klammerausdruecke werden wie in der Vorlage nicht unterstuetzt.

struct ExprParser {
    string s;
    size_t i = 0;

    explicit ExprParser(string input)
        : s(std::move(input))
    {
    }

    void skip_ws()
    {
        while (i < s.size()
               && isspace(static_cast<unsigned char>(s[i]))) {
            ++i;
        }
    }

    bool match(char c)
    {
        skip_ws();

        if (i < s.size() && s[i] == c) {
            ++i;
            return true;
        }

        return false;
    }

    double parse_factor()
    {
        skip_ws();

        if (match('+')) {
            return parse_factor();
        }

        if (match('-')) {
            return -parse_factor();
        }

        if (i + 1 < s.size()
            && (s[i] == 'p' || s[i] == 'P')
            && (s[i + 1] == 'i' || s[i + 1] == 'I')) {
            i += 2;
            return PI;
        }

        const size_t start = i;
        bool seen_digit = false;

        while (i < s.size()) {
            const char c = s[i];

            if (isdigit(static_cast<unsigned char>(c))) {
                seen_digit = true;
                ++i;
                continue;
            }

            if (c == '.') {
                ++i;
                continue;
            }

            if (c == 'e' || c == 'E') {
                ++i;

                if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
                    ++i;
                }

                continue;
            }

            break;
        }

        if (!seen_digit) {
            throw runtime_error("Kein gueltiger Faktor gefunden.");
        }

        const string token = s.substr(start, i - start);
        istringstream iss(token);
        iss.imbue(locale::classic());

        double value = 0.0;
        iss >> value;

        if (!iss) {
            throw runtime_error("Zahl konnte nicht gelesen werden.");
        }

        // Verhindert, dass beispielsweise 1.2.3 teilweise akzeptiert wird.
        char extra;
        if (iss >> extra) {
            throw runtime_error("Ungueltiges Zahlenformat.");
        }

        return value;
    }

    double parse_term()
    {
        double x = parse_factor();

        while (true) {
            if (match('*')) {
                x *= parse_factor();
            } else if (match('/')) {
                const double divisor = parse_factor();

                if (divisor == 0.0) {
                    throw runtime_error("Division durch 0.");
                }

                x /= divisor;
            } else {
                break;
            }
        }

        return x;
    }

    double parse_expr()
    {
        double x = parse_term();

        while (true) {
            if (match('+')) {
                x += parse_term();
            } else if (match('-')) {
                x -= parse_term();
            } else {
                break;
            }
        }

        return x;
    }

    double parse_all()
    {
        const double value = parse_expr();
        skip_ws();

        if (i != s.size()) {
            throw runtime_error("Unerwartete Zeichen am Ende.");
        }

        if (!isfinite(value)) {
            throw runtime_error("Ergebnis ist nicht endlich.");
        }

        return value;
    }
};

bool parse_double_with_pi(const string& input, double& out)
{
    string s = trim(input);

    if (s.empty()) {
        return false;
    }

    replace_all(s, ",", ".");

    try {
        ExprParser parser(s);
        out = parser.parse_all();
        return true;
    } catch (const exception&) {
        return false;
    }
}

bool read_double_smart(
    const string& prompt,
    double& value,
    double default_value,
    bool allow_default = true)
{
    while (true) {
        cout << prompt;

        if (allow_default) {
            cout << " [default: " << setprecision(10)
                 << default_value << "]";
        }

        cout << ": " << flush;

        string line;

        if (!getline(cin, line)) {
            return false;
        }

        line = trim(line);

        if (line.empty() && allow_default) {
            value = default_value;
            return true;
        }

        double parsed = 0.0;

        if (parse_double_with_pi(line, parsed)) {
            value = parsed;
            return true;
        }

        cout << "Ungueltige Eingabe. Beispiele: "
             << "0.01745, pi/180, 2*pi, 10*pi/180, 0,017\n";
    }
}

bool read_string_smart(
    const string& prompt,
    string& value,
    const string& default_value,
    bool allow_default = true)
{
    cout << prompt;

    if (allow_default) {
        cout << " [default: " << default_value << "]";
    }

    cout << ": " << flush;

    string line;

    if (!getline(cin, line)) {
        return false;
    }

    line = trim(line);
    value = (line.empty() && allow_default) ? default_value : line;

    return true;
}

bool ask_yes_no(const string& prompt, bool default_yes)
{
    cout << prompt << " (" << (default_yes ? "Y/n" : "y/N")
         << "): " << flush;

    string s;

    if (!getline(cin, s)) {
        throw runtime_error("Eingabe abgebrochen.");
    }

    s = trim(s);

    if (s.empty()) {
        return default_yes;
    }

    const char c =
        static_cast<char>(tolower(static_cast<unsigned char>(s[0])));

    if (c == 'y' || c == 'j') {
        return true;
    }

    if (c == 'n') {
        return false;
    }

    return default_yes;
}

// Wiebe-Funktion und vereinfachter Druckansatz

double wiebe_xb(double phi, const Params& p)
{
    const double start = p.phi0_rad;
    const double end = start + p.dphi_comb_rad;

    if (phi < start) {
        return 0.0;
    }

    if (phi > end) {
        return 1.0;
    }

    const double theta = (phi - start) / (end - start);

    return 1.0 - exp(-p.a * pow(theta, p.m + 1.0));
}

double p_z(double phi, const Params& p)
{
    const double sigma = 40.0 * PI / 180.0;
    const double s = exp(-(phi * phi) / (sigma * sigma));

    const double p_base =
        p.p_int + (p.p_tdc_base - p.p_int) * s;

    const double xb = wiebe_xb(phi, p);

    const double ratio =
        (p.p_tdc_base > 0.0) ? p.p_max / p.p_tdc_base : 1.0;

    double pressure = p_base * (1.0 + (ratio - 1.0) * xb);

    if (pressure < p.p_int) {
        pressure = p.p_int;
    }

    return pressure;
}

// Kraftfunktionen: Formeln aus der Vorlage unveraendert

double oszillierendeMassenkraefte(
    double mo, double r, double omega, double phi, double lambda)
{
    return mo * r * omega * omega * cos(phi)
         + lambda * mo * r * omega * omega * cos(phi) * cos(phi);
}

double Fluidkraefte(double pz, double Ak, double p0)
{
    return (pz - p0) * Ak;
}

double Kolbenkraft(double FZ, double Fo)
{
    return FZ + Fo;
}

double Beta(double phi, double lambda)
{
    double value = lambda * sin(phi);

    if (value > 1.0) {
        value = 1.0;
    }

    if (value < -1.0) {
        value = -1.0;
    }

    return asin(value);
}

double Radialkraft(double FK, double phi, double beta)
{
    return FK * cos(phi + beta) / cos(beta);
}

double Tangentialkraft(double FK, double phi, double beta)
{
    return FK * sin(phi + beta) / cos(beta);
}

double rotierendeMassenkraft(double mpr, double r, double omega)
{
    return mpr * r * omega * omega;
}

double Kurbelzapfenkraft(double FKLx, double FKLy)
{
    return sqrt(FKLx * FKLx + FKLy * FKLy);
}

double FKWr(
    double mw, double rw, double r, double mp,
    double l, double rp, double mz, double omega)
{
    const double mwred = mw * rw / r;
    const double mpr_red = mp * (l - rp) / l;
    const double mr = mpr_red + mz + mwred;

    return mr * r * omega * omega;
}

double Wellenkraft(double FMx, double FMy)
{
    return sqrt(FMx * FMx + FMy * FMy);
}

// Richtungszerlegung

double Radial_x(double FR, double phi)
{
    return FR * cos(phi);
}

double Radial_y(double FR, double phi)
{
    return FR * sin(phi);
}

double Tangential_x(double FT, double phi)
{
    return -FT * sin(phi);
}

double Tangential_y(double FT, double phi)
{
    return FT * cos(phi);
}

double Rotierend_x(double F, double phi)
{
    return F * cos(phi);
}

double Rotierend_y(double F, double phi)
{
    return F * sin(phi);
}

// Speichert die Eingaben, liest input.dat jedoch nicht ein.

void write_input_dat(const Params& p)
{
    ofstream f(p.input_filename);

    if (!f) {
        throw runtime_error(
            "Kann Eingabedatei nicht schreiben: " + p.input_filename);
    }

    f.imbue(locale::classic());
    f << setprecision(16)
      << "# Eingabedatei fuer pz und FM\n"
      << "phi_start_rad " << p.phi_start_rad << "\n"
      << "phi_end_rad " << p.phi_end_rad << "\n"
      << "dphi_rad " << p.dphi_rad << "\n"
      << "p_int_Pa " << p.p_int << "\n"
      << "p_exh_Pa " << p.p_exh << "\n"
      << "p0_Pa " << p.p0 << "\n"
      << "p_max_Pa " << p.p_max << "\n"
      << "p_tdc_base_Pa " << p.p_tdc_base << "\n"
      << "phi0_rad " << p.phi0_rad << "\n"
      << "dphi_comb_rad " << p.dphi_comb_rad << "\n"
      << "wiebe_a " << p.a << "\n"
      << "wiebe_m " << p.m << "\n"
      << "Ak " << p.Ak << "\n"
      << "mo " << p.mo << "\n"
      << "r " << p.r << "\n"
      << "omega " << p.omega << "\n"
      << "lambda " << p.lambda << "\n"
      << "mpr " << p.mpr << "\n"
      << "mw " << p.mw << "\n"
      << "rw " << p.rw << "\n"
      << "mp " << p.mp << "\n"
      << "l " << p.l << "\n"
      << "rp " << p.rp << "\n"
      << "mz " << p.mz << "\n"
      << "csv_filename " << p.csv_filename << "\n"
      << "csv_sep " << p.csv_sep << "\n"
      << "write_deg " << (p.write_deg_column ? 1 : 0) << "\n"
      << "write_xb " << (p.write_xb_column ? 1 : 0) << "\n";

    f.close();

    if (!f) {
        throw runtime_error("Fehler beim Schreiben von input.dat.");
    }
}

int main()
{
    try {
        Params p;

        ios::sync_with_stdio(false);
        cin.tie(nullptr);
        cout.imbue(locale::classic());

        cout << "=== p_z(phi) + Wellenkraft FM Generator ===\n"
             << "Winkel phi in rad, OT bei phi = 0.\n"
             << "Eingaben: pi/180, 2*pi, 10*pi/180, 0,017\n"
             << "Enter uebernimmt jeweils den Standardwert.\n\n";

        // EOF wird abgefangen, statt unbemerkt weiterzurechnen.
        auto read_value = [](const string& prompt, double& value) {
            if (!read_double_smart(prompt, value, value)) {
                throw runtime_error("Eingabe abgebrochen.");
            }
        };

        read_value("phi_start [rad]", p.phi_start_rad);
        read_value("phi_end [rad]", p.phi_end_rad);
        read_value("dphi [rad]", p.dphi_rad);

        if (p.dphi_rad <= 0.0) {
            throw runtime_error("dphi muss > 0 sein.");
        }

        if (p.phi_end_rad <= p.phi_start_rad) {
            throw runtime_error("phi_end muss > phi_start sein.");
        }

        read_value("p_int [Pa], Ansaugdruck absolut", p.p_int);
        read_value("p0 [Pa], Referenzdruck", p.p0);
        read_value("p_max [Pa], Druckparameter", p.p_max);
        read_value("p_TDC_base [Pa], Basisdruck am OT", p.p_tdc_base);
        read_value("p_exh [Pa], Abgasdruck", p.p_exh);

        double phi0_deg = p.phi0_rad * 180.0 / PI;
        double dphi_comb_deg = p.dphi_comb_rad * 180.0 / PI;

        read_value("Wiebe: phi0 [deg]", phi0_deg);
        read_value("Wiebe: dphi_comb [deg]", dphi_comb_deg);

        p.phi0_rad = phi0_deg * PI / 180.0;
        p.dphi_comb_rad = dphi_comb_deg * PI / 180.0;

        read_value("Wiebe: a", p.a);
        read_value("Wiebe: m", p.m);

        if (p.dphi_comb_rad <= 0.0 || p.a <= 0.0 || p.m <= -1.0) {
            throw runtime_error(
                "Wiebe erfordert dphi_comb > 0, a > 0 und m > -1.");
        }

        cout << "\nKraftberechnung:\n";

        read_value("Ak [m^2]", p.Ak);
        read_value("mo [kg]", p.mo);
        read_value("r [m]", p.r);
        read_value("omega [rad/s]", p.omega);
        read_value("lambda [-]", p.lambda);
        read_value("mpr [kg]", p.mpr);
        read_value("mw [kg]", p.mw);
        read_value("rw [m]", p.rw);
        read_value("mp [kg]", p.mp);
        read_value("l [m]", p.l);
        read_value("rp [m]", p.rp);
        read_value("mz [kg]", p.mz);

        if (p.r <= 0.0 || p.l <= 0.0 || p.Ak <= 0.0) {
            throw runtime_error("r, l und Ak muessen > 0 sein.");
        }

        if (p.lambda < 0.0 || p.lambda >= 1.0) {
            throw runtime_error("lambda muss zwischen 0 und 1 liegen.");
        }

        if (abs(p.lambda - p.r / p.l) > 1e-6) {
            cerr << "Hinweis: lambda stimmt nicht mit r/l ueberein.\n";
        }

        cout << "\nCSV-Ausgabe:\n";

        if (!read_string_smart(
                "Dateiname", p.csv_filename, p.csv_filename)) {
            throw runtime_error("Eingabe abgebrochen.");
        }

        p.write_deg_column =
            ask_yes_no("Grad-Spalte mitschreiben?", true);
        p.write_xb_column =
            ask_yes_no("Wiebe-Umsatz x_b mitschreiben?", true);

        string separator;

        if (!read_string_smart(
                "Trennzeichen: ',' oder ';'",
                separator,
                string(1, p.csv_sep))) {
            throw runtime_error("Eingabe abgebrochen.");
        }

        if (separator.size() != 1
            || (separator[0] != ',' && separator[0] != ';')) {
            throw runtime_error("Ungueltiges CSV-Trennzeichen.");
        }

        p.csv_sep = separator[0];

        if (p.csv_filename == p.input_filename) {
            throw runtime_error(
                "CSV-Datei und Eingabedatei muessen verschieden sein.");
        }

        write_input_dat(p);
        cout << "\nSchreibe " << p.input_filename << "\n";

        ofstream csv(p.csv_filename);

        if (!csv) {
            throw runtime_error(
                "Kann CSV-Datei nicht schreiben: " + p.csv_filename);
        }

        // Dezimalpunkt, unabhaengig von der System-Locale.
        csv.imbue(locale::classic());
        csv << setprecision(10) << fixed;

        csv << "phi_rad";

        if (p.write_deg_column) {
            csv << p.csv_sep << "phi_deg";
        }

        csv << p.csv_sep << "p_Pa"
            << p.csv_sep << "p_bar";

        if (p.write_xb_column) {
            csv << p.csv_sep << "xb";
        }

        csv << p.csv_sep << "beta_rad"
            << p.csv_sep << "Fo_N"
            << p.csv_sep << "FZ_N"
            << p.csv_sep << "FK_N"
            << p.csv_sep << "FR_N"
            << p.csv_sep << "FT_N"
            << p.csv_sep << "FPR_N"
            << p.csv_sep << "FKWr_N"
            << p.csv_sep << "FKLx_N"
            << p.csv_sep << "FKLy_N"
            << p.csv_sep << "FMx_N"
            << p.csv_sep << "FMy_N"
            << p.csv_sep << "FKL_N"
            << p.csv_sep << "FM_N\n";

        const int max_steps = 50000000;
        int step = 0;

        for (double phi = p.phi_start_rad;
             phi <= p.phi_end_rad + 0.5 * p.dphi_rad;
             phi += p.dphi_rad) {

            if (++step > max_steps) {
                throw runtime_error(
                    "Zu viele Schritte. CSV enthaelt nur Teilergebnisse.");
            }

            if (phi + p.dphi_rad == phi) {
                throw runtime_error(
                    "Winkelschritt ist numerisch zu klein.");
            }

            const double pressure = p_z(phi, p);
            const double pbar = pressure / 1e5;
            const double phideg = phi * 180.0 / PI;
            const double xb =
                p.write_xb_column ? wiebe_xb(phi, p) : 0.0;

            const double beta = Beta(phi, p.lambda);
            const double Fo = oszillierendeMassenkraefte(
                p.mo, p.r, p.omega, phi, p.lambda);

            const double FZ = Fluidkraefte(pressure, p.Ak, p.p0);
            const double FK = Kolbenkraft(FZ, Fo);
            const double FR = Radialkraft(FK, phi, beta);
            const double FT = Tangentialkraft(FK, phi, beta);

            const double FPR =
                rotierendeMassenkraft(p.mpr, p.r, p.omega);

            const double FKWrWert = FKWr(
                p.mw, p.rw, p.r, p.mp, p.l, p.rp, p.mz, p.omega);

            const double FRx = Radial_x(FR, phi);
            const double FRy = Radial_y(FR, phi);
            const double FTx = Tangential_x(FT, phi);
            const double FTy = Tangential_y(FT, phi);
            const double FPRx = Rotierend_x(FPR, phi);
            const double FPRy = Rotierend_y(FPR, phi);

            const double FKLx = FRx + FTx + FPRx;
            const double FKLy = FRy + FTy + FPRy;

            const double FKWRx = Rotierend_x(FKWrWert, phi);
            const double FKWRy = Rotierend_y(FKWrWert, phi);

            const double FMx = FKLx + FKWRx;
            const double FMy = FKLy + FKWRy;

            const double FKL = Kurbelzapfenkraft(FKLx, FKLy);
            const double FM = Wellenkraft(FMx, FMy);

            cout << "Schritt " << step
                 << " | phi = " << phi
                 << " rad | pz = " << pressure
                 << " Pa | FMx = " << FMx
                 << " N | FMy = " << FMy
                 << " N | FM = " << FM << " N\n";

            csv << phi;

            if (p.write_deg_column) {
                csv << p.csv_sep << phideg;
            }

            csv << p.csv_sep << pressure
                << p.csv_sep << pbar;

            if (p.write_xb_column) {
                csv << p.csv_sep << xb;
            }

            csv << p.csv_sep << beta
                << p.csv_sep << Fo
                << p.csv_sep << FZ
                << p.csv_sep << FK
                << p.csv_sep << FR
                << p.csv_sep << FT
                << p.csv_sep << FPR
                << p.csv_sep << FKWrWert
                << p.csv_sep << FKLx
                << p.csv_sep << FKLy
                << p.csv_sep << FMx
                << p.csv_sep << FMy
                << p.csv_sep << FKL
                << p.csv_sep << FM
                << "\n";
        }

        csv.close();

        if (!csv) {
            throw runtime_error("Fehler beim Schreiben der CSV-Datei.");
        }

        cout << "\nFertig. CSV geschrieben: " << p.csv_filename
             << "\nTrennzeichen: "
             << (p.csv_sep == ';' ? "Semikolon" : "Komma")
             << "; Dezimalzeichen: Punkt.\n";

        return 0;
    } catch (const exception& e) {
        cerr << "\nFehler: " << e.what() << "\n";
        return 1;
    }
}
