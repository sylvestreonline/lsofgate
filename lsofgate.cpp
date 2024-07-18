#include <iostream>
#include <fstream>
#include <array>
#include <string>
#include <regex>
#include <map>
#include <stdexcept>
#include <unistd.h> // pour sleep()

// Exécute une commande et capture la sortie
std::string executeCommand(const char* cmd) {
    FILE* pipe = popen(cmd, "r");
    if (!pipe) throw std::runtime_error("popen() failed!");
    std::array<char, 256> buffer;
    std::string result;
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    return result;
}

// Obtient l'adresse IP locale du serveur
std::string getLocalIP() {
    return executeCommand("hostname -I | awk '{print $1}'");
}

int main() {
    std::ofstream logFile("ssh_monitor.log", std::ios::app); // Ouverture du fichier de log
    if (!logFile) {
        throw std::runtime_error("Unable to open log file.");
    }

    std::string myIP = getLocalIP(); // Obtient l'IP du serveur
    logFile << "IP du serveur: " << myIP << std::endl;

    std::map<std::string, int> ipCount; // Pour compter les connexions par IP

    while (true) {
        std::string lsofOutput = executeCommand("lsof -i TCP:22 -sTCP:ESTABLISHED");
        std::regex lineRegex(R"((\S+)\s+(\d+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+))");
        std::smatch matches;
        auto searchStart = lsofOutput.cbegin();

        while (std::regex_search(searchStart, lsofOutput.cend(), matches, lineRegex)) {
            std::string process = matches[1];
            std::string pid = matches[2];
            std::string state = matches[8];
            std::string name = matches[9];

            // Extraire et traiter l'adresse IP
            std::regex ipRegex(R"(.*->(.*):\d+)");
            std::smatch ipMatches;
            if (std::regex_search(name, ipMatches, ipRegex) && ipMatches.size() > 1) {
                std::string ip = ipMatches[1];
                ipCount[ip]++;
                if (ipCount[ip] > 3) { // Exemple de seuil pour la détection
                    std::string killCommand = "kill -9 " + pid;
                    executeCommand(killCommand.c_str());
                    logFile << "Terminé processus suspect PID: " << pid << " pour IP: " << ip << std::endl;
                } else {
                    logFile << "Connexion valide établie par PID : " << pid << " (" << process << ") de l'IP: " << ip << std::endl;
                }
            }
            searchStart = matches.suffix().first;
        }
        sleep(10); // Vérifie toutes les 10 secondes
    }
    return 0;
}