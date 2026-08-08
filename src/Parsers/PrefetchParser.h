#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace Parsers {

    struct PrefetchEntry {
        std::string ExecutableName;   // Nom du .exe (ex: "chrome.exe")
        std::string LastRunTime;      // Dernier lancement (formaté)
        uint64_t    LastRunTimestamp; // Pour le tri
        std::string PrefetchFile;     // Chemin complet du .pf
        std::string ResolvedPath;     // Chemin complet trouvé sur le disque (vide si introuvable)
        bool        ExistsOnDisk;     // True si le .exe a été trouvé
        bool        IsSigned;         // True si le .exe est signé

        // Historique d'exécution (jusqu'à 8 pour Win10/11)
        std::vector<std::string> RunHistory;

        // Métadonnées du fichier .pf
        std::string PfSize;
        std::string PfCreated;
        std::string PfModified;

        // Métadonnées de l'exécutable
        std::string ExeSize;
        std::string ExeCreated;
        std::string ExeModified;
    };

    class PrefetchParser {
    public:
        static std::vector<PrefetchEntry> Parse();
    };

} // namespace Parsers
