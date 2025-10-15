#include <iostream>
#include <string>
#include <vector>
#include <regex>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <getopt.h>
#include <cstdint>

using namespace std;

// Tabla xlat (51 bytes)
const vector<uint8_t> xlat = {
    0x64, 0x73, 0x66, 0x64, 0x3b, 0x6b, 0x66, 0x6f, 0x41, 0x2c, 0x2e, 0x69,
    0x79, 0x65, 0x77, 0x72, 0x6b, 0x6c, 0x64, 0x4a, 0x4b, 0x44, 0x48, 0x53,
    0x55, 0x42, 0x73, 0x67, 0x76, 0x63, 0x61, 0x36, 0x39, 0x38, 0x33, 0x34,
    0x6e, 0x63, 0x78, 0x76, 0x39, 0x38, 0x37, 0x33, 0x32, 0x35, 0x34, 0x6b,
    0x3b, 0x66, 0x67, 0x38, 0x37
};

// Decrypt Cisco type 7 password
string decrypt_type7(const string& encrypted) {
    if (encrypted.size() < 2) return "";

    int s = stoi(encrypted.substr(0, 2)); // Salt
    string hexpart = encrypted.substr(2);
    string result;

    for (size_t i = 0; i < hexpart.size(); i += 2) {
        string byte_str = hexpart.substr(i, 2);
        uint8_t val = static_cast<uint8_t>(stoi(byte_str, nullptr, 16));
        result += static_cast<char>(val ^ xlat[s]);
        s = (s + 1) % xlat.size();
    }

    return result;
}

//   Encrypt Cisco type 7 password
string encrypt_type7(const string& plain) {
    int salt = rand() % 15;
    stringstream ss;
    ss << setw(2) << setfill('0') << salt;

    for (char c : plain) {
        uint8_t encrypted = static_cast<uint8_t>(c) ^ xlat[salt];
        ss << hex << setw(2) << setfill('0') << (int)encrypted;
        salt = (salt + 1) % xlat.size();
    }

    return ss.str();
}

// Procesar archivo de configuración
void process_file(const string& filename) {
    ifstream infile(filename);
    if (!infile) {
        cerr << "No se pudo abrir el archivo: " << filename << endl;
        return;
    }

    string line;
    regex regex_cisco(R"(7\s+([0-9A-Fa-f]+))");

    while (getline(infile, line)) {
        smatch match;
        if (regex_search(line, match, regex_cisco)) {
            string enc = match[1];
            string dec = decrypt_type7(enc);
            cout << "Contraseña desencriptada: " << dec << endl;
        }
    }
}

//  Mostrar ayuda
void show_help(const char* prog) {
    cout << "Uso: " << prog << " [opciones]\n"
         << "Opciones:\n"
         << "  -e            Encriptar contraseña\n"
         << "  -d            Desencriptar contraseña (por defecto)\n"
         << "  -p <pass>     Contraseña a encriptar o desencriptar\n"
         << "  -f <archivo>  Archivo de configuración de Cisco para desencriptar\n"
         << "  -h            Mostrar esta ayuda\n";
}

int main(int argc, char* argv[]) {
    srand(static_cast<unsigned int>(time(nullptr)));

    bool encrypt = false;
    bool decrypt = true;
    string password;
    string filename;

    int opt;
    while ((opt = getopt(argc, argv, "edp:f:h")) != -1) {
        switch (opt) {
            case 'e': encrypt = true; decrypt = false; break;
            case 'd': decrypt = true; break;
            case 'p': password = optarg; break;
            case 'f': filename = optarg; break;
            case 'h': show_help(argv[0]); return 0;
            default: show_help(argv[0]); return 1;
        }
    }

    if (!password.empty()) {
        if (decrypt) {
            string result = decrypt_type7(password);
            cout << "Contraseña desencriptada: " << result << endl;
        } else if (encrypt) {
            string result = encrypt_type7(password);
            cout << "Contraseña encriptada: " << result << endl;
        }
    } else if (!filename.empty()) {
        if (encrypt) {
            cerr << "No puedes encriptar un archivo de configuración." << endl;
            return 1;
        } else {
            process_file(filename);
        }
    } else {
        show_help(argv[0]);
        return 1;
    }

    return 0;
}
