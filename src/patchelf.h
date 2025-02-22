#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "elf.h"

using FileContents = std::shared_ptr<std::vector<unsigned char>>;

#define ElfFileParams class Elf_Ehdr, class Elf_Phdr, class Elf_Shdr, class Elf_Addr, class Elf_Off, class Elf_Dyn, class Elf_Sym, class Elf_Verneed, class Elf_Versym
#define ElfFileParamNames Elf_Ehdr, Elf_Phdr, Elf_Shdr, Elf_Addr, Elf_Off, Elf_Dyn, Elf_Sym, Elf_Verneed, Elf_Versym

template<ElfFileParams>
class ElfFile
{
public:

    const FileContents fileContents;

private:

    std::vector<Elf_Phdr> phdrs;
    std::vector<Elf_Shdr> shdrs;

    bool littleEndian;

    bool changed = false;

    bool isExecutable = false;

    using SectionName = std::string;
    using ReplacedSections = std::map<SectionName, std::string>;

    ReplacedSections replacedSections;

    std::string sectionNames; /* content of the .shstrtab section */

    /* Align on 4 or 8 bytes boundaries on 32- or 64-bit platforms
       respectively. */
    static constexpr size_t sectionAlignment = sizeof(Elf_Off);

    std::vector<SectionName> sectionsByOldIndex;

public:
    explicit ElfFile(FileContents fileContents);

    [[nodiscard]] bool isChanged() const noexcept
    {
        return changed;
    }

private:

    struct CompPhdr
    {
        const ElfFile * elfFile;
        bool operator ()(const Elf_Phdr & x, const Elf_Phdr & y) const noexcept
        {
            // A PHDR comes before everything else.
            if (elfFile->rdi(y.p_type) == PT_PHDR) return false;
            if (elfFile->rdi(x.p_type) == PT_PHDR) return true;

            // Sort non-PHDRs by address.
            return elfFile->rdi(x.p_paddr) < elfFile->rdi(y.p_paddr);
        }
    };

    void sortPhdrs();

    struct CompShdr
    {
        const ElfFile * elfFile;
        bool operator ()(const Elf_Shdr & x, const Elf_Shdr & y) const noexcept
        {
            return elfFile->rdi(x.sh_offset) < elfFile->rdi(y.sh_offset);
        }
    };

    [[nodiscard]] unsigned int getPageSize() const noexcept;

    void sortShdrs();

    void shiftFile(unsigned int extraPages, size_t sizeOffset, size_t extraBytes);

    [[nodiscard]] std::string getSectionName(const Elf_Shdr & shdr) const;

    const Elf_Shdr & findSectionHeader(const SectionName & sectionName) const;

    [[nodiscard]] std::optional<std::reference_wrapper<const Elf_Shdr>> tryFindSectionHeader(const SectionName & sectionName) const;

    [[nodiscard]] unsigned int getSectionIndex(const SectionName & sectionName) const;

    std::string & replaceSection(const SectionName & sectionName,
        unsigned int size);

    [[nodiscard]] bool haveReplacedSection(const SectionName & sectionName) const;

    void writeReplacedSections(Elf_Off & curOff,
        Elf_Addr startAddr, Elf_Off startOffset);

    void rewriteHeaders(Elf_Addr phdrAddress);

    void rewriteSectionsLibrary();

    void rewriteSectionsExecutable();

    void normalizeNoteSegments();

public:

    void rewriteSections(bool force = false);

    [[nodiscard]] std::string getInterpreter() const;

    typedef enum { printOsAbi, replaceOsAbi } osAbiMode;

    void modifyOsAbi(osAbiMode op, const std::string & newOsAbi);

    typedef enum { printSoname, replaceSoname } sonameMode;

    void modifySoname(sonameMode op, const std::string & newSoname);

    void setInterpreter(const std::string & newInterpreter);

    typedef enum { rpPrint, rpShrink, rpSet, rpAdd, rpRemove } RPathOp;

    void modifyRPath(RPathOp op, const std::vector<std::string> & allowedRpathPrefixes, std::string newRPath);
    std::string shrinkRPath(char* rpath, std::vector<std::string> &neededLibs, const std::vector<std::string> & allowedRpathPrefixes);
    void removeRPath(Elf_Shdr & shdrDynamic);

    void addNeeded(const std::set<std::string> & libs);

    void removeNeeded(const std::set<std::string> & libs);

    void replaceNeeded(const std::map<std::string, std::string> & libs);

    void printNeededLibs() const;

    void noDefaultLib();

    void addDebugTag();

    void clearSymbolVersions(const std::set<std::string> & syms);

    enum class ExecstackMode { print, set, clear };

    void modifyExecstack(ExecstackMode op);

private:

    /* Convert an integer in big or little endian representation (as
       specified by the ELF header) to this platform's integer
       representation. */
    template<class I>
    constexpr I rdi(I i) const noexcept;

    /* Convert back to the ELF representation. */
    template<class I, class U>
    constexpr inline I wri(I & t, U i) const
    {
        I val = static_cast<I>(i);
        if (static_cast<U>(val) != i)            
            throw std::runtime_error { "value truncation" };
        t = rdi(val);
        return val;
    }

    [[nodiscard]] Elf_Ehdr *hdr() noexcept {
      return reinterpret_cast<Elf_Ehdr *>(fileContents->data());
    }

    [[nodiscard]] const Elf_Ehdr *hdr() const noexcept {
      return reinterpret_cast<const Elf_Ehdr *>(fileContents->data());
    }
};
