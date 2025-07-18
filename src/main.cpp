#include <filesystem>
#include <iostream>
#include <vector>
#include <string>

#include <thread>
#include <mutex>

#include <fstream>

#include <locale>

namespace fs = std::filesystem;

struct TwoStrings
{
    std::string first;
    std::string second;
};

bool isPresent(const std::vector<TwoStrings>& vec, const std::string& val, size_t* foundIndex)
{
    for (size_t i = 0; i < vec.size(); i++)
    {
        if (vec[i].first == val)
        {
            if (foundIndex)
                *foundIndex = i;
            return true;
        }
    }
    return false;
}

void readMapping(const fs::directory_entry& entry, std::vector<TwoStrings>& mappings, bool verboseOutput)
{
    std::ifstream file(entry.path());
    if (!file.is_open())
    {
        std::cout << "Cannot open file " << entry.path();
        return;
    }
    std::string line;
    while (std::getline(file, line))
    {
        bool skip = true;
        bool CLASS = false;
        size_t pointer = 0;
        if (line[pointer] == '	' || line[pointer] == ' ')
            pointer++;

        if (line.size() - pointer < 5)
            continue;

        if (memcmp(line.data() + pointer, "CLASS", 5) == 0)
        {
            skip = false;
            if (pointer == 0)
            {
                CLASS = true;
            }
            pointer += 5;

        }
        if (skip && memcmp(line.data() + pointer, "FIELD", 5) == 0) { skip = false; pointer += 5; }
        if (skip && line.size() - pointer < 6)
            continue;

        if (skip && memcmp(line.data() + pointer, "METHOD", 6) == 0) { skip = false; pointer += 6; }
        if (skip)
            continue;

        while (line[pointer] == ' ' || line[pointer] == '	')
            pointer++;

        if (CLASS)
        {
            size_t start = pointer;
            while (line[pointer] != ' ')
            {
                if (pointer == line.size())
                    break;

                pointer++;
            }
            if (pointer == line.size())
                continue;

            std::string obfuscatedName = std::string(line.data() + start, pointer - start);
            std::replace(obfuscatedName.begin(), obfuscatedName.end(), '/', '.');
            std::string obfuscatedNameEnd;
            std::istringstream obfuscatedNameIS = std::istringstream(obfuscatedName);
            while (std::getline(obfuscatedNameIS, obfuscatedNameEnd, '.'));

            pointer++;
            char sssd = line[pointer];
            start = pointer;
            pointer = line.size() - start;

            std::string mappedName = std::string(line.data() + start, pointer);
            std::replace(mappedName.begin(), mappedName.end(), '/', '.');
            std::string mappedNameEnd;
            std::istringstream mappedNameIS = std::istringstream(mappedName);
            while (std::getline(mappedNameIS, mappedNameEnd, '.'));

            size_t index = 0;
            if (isPresent(mappings, obfuscatedName, &index))
                if(verboseOutput) std::cout << "Found similar names: " << obfuscatedName << '-' << mappings[index].first << '\n';
            else
                mappings.push_back({ std::move(obfuscatedName), std::move(mappedName) });

            if (isPresent(mappings, obfuscatedNameEnd, &index))
                if(verboseOutput) std::cout << "Found similar names: " << obfuscatedNameEnd << '-' << mappings[index].first << '\n';
            else
                mappings.push_back({ std::move(obfuscatedNameEnd), std::move(mappedNameEnd) });
        }
        else {
            size_t start = pointer;
            while (pointer != line.size() && line[pointer] != ' ')
                pointer++;
            if (pointer == line.size())
                continue;

            std::string obfuscatedName = std::string(line.data() + start, pointer - start);
            if (obfuscatedName == "<init>")
                continue;

            pointer++;
            start = pointer;
            while (pointer != line.size() && line[pointer] != ' ')
                pointer++;

            std::string mappedName = std::string(line.data() + start, pointer - start);
            if (mappedName[0] == '(')
                continue;

            size_t index = 0;
            if (isPresent(mappings, obfuscatedName, &index))
                if(verboseOutput) std::cout << "Found similar names: " << obfuscatedName << '-' << mappings[index].first << '\n';
            else
                mappings.push_back({ std::move(obfuscatedName), std::move(mappedName) });
        }
    }
}

void mapNames(const fs::directory_entry& entry, std::vector<TwoStrings>& mappings, bool verboseOutput)
{
    std::ifstream inFile(entry.path());
    if (!inFile.is_open())
    {
        std::cout << "Cannot open file for input " << entry.path() << '\n';
        return;
    }

    std::vector<std::string> fileLines;
    std::string curLine;
    while (std::getline(inFile, curLine))
    {
        fileLines.push_back(std::move(curLine));
    }
    inFile.close();

    for (auto& line : fileLines)
    {
        for (const auto& mapped : mappings)
        {
            bool skip = false;
            size_t offset = 0;
            while (!skip)
            {
                std::string offsetedLine = line.c_str() + offset;
                size_t pos = offsetedLine.find(mapped.first);
                if (pos == std::string::npos)
                {
                    if (pos + mapped.first.size() < line.size())
                        offset += pos + mapped.first.size();
                    else
                        skip = true;

                    break;
                }
                pos -= offset;

                size_t pos2 = pos + mapped.first.size();
                if (pos2 < line.size() && std::isdigit(line[pos + mapped.first.size()]))
                {
                    if (pos + mapped.first.size() < line.size())
                        offset += pos + mapped.first.size();
                    else
                        skip = true;
                    break;
                }

                if(verboseOutput)
                    std::cout << mapped.first << " -> " << mapped.second << '\n';

                size_t newSize = line.size() - mapped.first.size() + mapped.second.size();
                std::string newLine;
                newLine.resize(newSize);

                for (size_t i = 0; i < pos; i++)
                    newLine[i] = line[i];

                for (size_t i = 0; i < mapped.second.size(); i++)
                    newLine[pos + i] = mapped.second[i];

                for (size_t i = 0; i < newLine.size() - pos - mapped.second.size(); i++)
                    newLine[pos + mapped.second.size() + i] = line[pos + mapped.first.size() + i];

                line = std::move(newLine);
            }
        }
    }

    std::ofstream outFile(entry.path(), std::ios::trunc);
    if (!outFile.is_open())
    {
        std::cout << "Cannot open file for output " << entry.path() << '\n';
    }

    for (const auto& line : fileLines)
    {
        outFile << line << '\n';
    }
    outFile.close();
}

int main(void)
{
    fs::path mappingsPath = R"(C:\Users\VisualGoose\Desktop\yarn-1.20.2\mappings)";
    std::cout << "Mappings path (for example <some path>/yarn-1.20.2/mappings\n" <<
        "Path: ";
    std::cin >> mappingsPath;
    std::cout << "Src code path (for example <project root path>/src/main/java/<package>\n" <<
        "Path: ";
    fs::path projPath = R"(C:\Users\VisualGoose\IdeaProjects\WouldYouRather-Backup\src\main\java\com\eightsidedsquare\wyr)";
    std::cin >> projPath;

    std::cout << "Enable verbose output? (will significantly reduce performance)\n" <<
        "(Y/N): ";
    std::string verboseOutputString;
    std::cin >> verboseOutputString;
    bool verboseOutput = false; //std::cout is slow, so give user option to disable the extensive use of it
    if (!verboseOutputString.empty())
    {
        if (verboseOutputString[0] == 'Y')
            verboseOutput = true;
    }

    if (!fs::exists(mappingsPath) || !fs::exists(projPath))
    {
        std::cout << "Invalid path\n";
        return -1;
    }
    std::vector<TwoStrings> mappings;
    mappings.reserve(512);

    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(mappingsPath))
    {
        if (entry.path().extension() != ".mapping")
            continue;

        if (verboseOutput)
            std::cout << "Reading " << entry.path().string() << '\n';
        readMapping(entry, mappings, verboseOutput);
    }

    std::cout << "got all the mappings!\n";
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(projPath))
    {
        if (entry.path().extension() != ".java")
            continue;

        if(verboseOutput)
            std::cout << "Mapping " << entry.path() << '\n';
        mapNames(entry, mappings, verboseOutput);
    }
    std::cout << "mapped!";
}