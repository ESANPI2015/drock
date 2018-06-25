#include "BasicModel.hpp"
#include "HypergraphYAML.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>
#include <getopt.h>

static struct option long_options[] = {
    {"help", no_argument, 0, 'h'},
    {0,0,0,0}
};

void usage (const char *myName)
{
    std::cout << "Usage:\n";
    std::cout << myName << " <yaml-file-in> <yaml-file-out> (<yaml-file-in>)\n\n";
    std::cout << "Options:\n";
    std::cout << "--help\t" << "Show usage\n";
    std::cout << "\nExample:\n";
    std::cout << myName << "drock-basic-model-from-db.yml drock-domain-as-hypergraph.yml\n";
    std::cout << myName << "drock-basic-model-from-db.yml drock-domain-as-hypergraph.yml other-hypergraph.yml\n";
}

// This tool takes a language definition and tries to interpret a given domain specific format given that definition
int main (int argc, char **argv)
{

    // Parse command line
    int c;
    while (1)
    {
        int option_index = 0;
        c = getopt_long(argc, argv, "h", long_options, &option_index);
        if (c == -1)
            break;

        switch (c)
        {
            case 'h':
            case '?':
                break;
            default:
                std::cout << "W00t?!\n";
                return 1;
        }
    }

    if ((argc - optind) < 2)
    {
        usage(argv[0]);
        return 1;
    }

    // Set vars
    std::string fileNameIn(argv[optind]);
    std::string fileNameOut(argv[optind+1]);

    // Load file and convert to string
    std::ifstream fin;
    fin.open(fileNameIn);
    if(!fin.good()) {
        std::cout << "READ FAILED\n";
        return 2;
    }
    std::stringstream ss;
    ss << fin.rdbuf();

    if ((argc - optind) > 2)
    {
        std::string fileNameIn2(argv[optind+2]);
        Hypergraph hg(YAML::LoadFile(fileNameIn2).as<Hypergraph>());
        Drock::Model dc(hg);

        // Call domain specific import
        dc.domainSpecificImport(ss.str());

        // Store imported graph
        std::ofstream fout;
        fout.open(fileNameOut);
        if(!fout.good()) {
            std::cout << "WRITE FAILED\n";
            return 3;
        }
        fout << YAML::StringFrom(dc) << std::endl;
        fout.close();
        fin.close();
    } else {
        Drock::Model dc;

        // Call domain specific import
        dc.domainSpecificImport(ss.str());

        // Store imported graph
        std::ofstream fout;
        fout.open(fileNameOut);
        if(!fout.good()) {
            std::cout << "WRITE FAILED\n";
            return 3;
        }
        fout << YAML::StringFrom(dc) << std::endl;
        fout.close();
        fin.close();
    }

    return 0;
}
