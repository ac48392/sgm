#include <QApplication>
#include <QSettings>
#include <QSurfaceFormat>
#include <QFont>
#include <QMetaObject>
#include <iostream>

#include "MainWindow.hpp"
#include "SGMGraphicsWidget.hpp"

#include "SGMEntityFunctions.h"
#include "SGMPrimitives.h"
#include "SGMTranslators.h"
#include "SGMComplex.h"

std::string GetExecutableName(const char *path_name)
{
    // strip off any path to get basename of file
    // find the last '\' or '/' characters
    const char * pos = strrchr(path_name,'/');
    if (pos == nullptr)
        {
        pos = strrchr(path_name,'\\');
        if (pos == nullptr)
            pos = path_name;
        else
            ++pos;
        }
    else
        {
        ++pos;
        }
    return std::string(pos);
}

#ifdef SGM_PRODUCT_VERSION
#define SGM_STR(s) #s
#define SGM_MAKE_VERSION_STRING(s) SGM_STR(s)
#else
#define SGM_MAKE_VERSION_STRING(s)
#endif

void PrintVersion()
    {
        std::cerr << "SGM version " << std::string(SGM_MAKE_VERSION_STRING(SGM_PRODUCT_VERSION)) << std::endl;
    }

// print short usage message

void PrintUsage(std::string sExecutableName)
    {
    std::cerr << "usage: " << std::endl;
    std::cerr << "    " << sExecutableName << " [-c] [-l FILE [FILE ...]] [FILE [FILE ...]]" << std::endl;
    }

// print longer help message

void PrintHelp(std::string sExecutableName)
    {
    PrintVersion();
    PrintUsage(sExecutableName);
    std::cerr << std::endl;
    std::cerr << "     -c --cover      read STL file and output an STL file with coordinate aligned planar gaps covered." << std::endl;
    std::cerr << "     -f --factures   read STL file and output an SGM file of the factures in a scaned sheet." << std::endl;
    std::cerr << "     -l --load       headlessly load each FILE via SGM::ReadFile and report OK/FAILED per file (no GUI)." << std::endl;
    }

void FindFractures(int argc, char **argv)
    {
    if (argc != 4)
        {
        std::cerr << "error: unable to determine input and output file names" << std::endl;
        PrintUsage(GetExecutableName(argv[0]));
        exit(1);
        }

    SGMInternal::thing *pThing = SGM::CreateThing();
    SGM::Result rResult(pThing);
    std::string InputSTLFile(argv[2]);
    std::vector<SGM::Entity> aEntities;
    std::vector<std::string> aLog;
    SGM::TranslatorOptions Options;
    Options.m_bMerge = true;
    SGM::ReadFile(rResult, InputSTLFile, aEntities, aLog, Options);
    auto resultType = rResult.GetResult();
    if (resultType != SGM::ResultTypeOK)
        {
        std::cerr << rResult.Message() << std::endl;
        exit(1);
        }
    std::vector<SGM::Complex> *aComplexes = (std::vector<SGM::Complex> *) &aEntities;
    SGM::Complex ComplexID = aComplexes->front();
    if (aComplexes->size() > 1)
        {
        ComplexID = SGM::MergeComplexes(rResult, *aComplexes);
        }
    std::vector<SGM::Complex> aHoles;
    SGM::FindHoles(rResult,ComplexID,aHoles);
    SGM::DeleteEntity(rResult,ComplexID); 
    std::string OutputSGMLFile(argv[3]);
    SGM::SaveSGM(rResult, OutputSGMLFile, SGM::Thing() , Options);
    }

// cover open surfaces on an STL file

void CoverSTL(int argc, char **argv)
    {
    if (argc != 4)
        {
        std::cerr << "error: unable to determine input and output file names" << std::endl;
        PrintUsage(GetExecutableName(argv[0]));
        exit(1);
        }

    SGMInternal::thing *pThing = SGM::CreateThing();
    SGM::Result rResult(pThing);
    std::string InputSTLFile(argv[2]);
    std::vector<SGM::Entity> aEntities;
    std::vector<std::string> aLog;
    SGM::TranslatorOptions Options;
    Options.m_bMerge = true;
    SGM::ReadFile(rResult, InputSTLFile, aEntities, aLog, Options);
    auto resultType = rResult.GetResult();
    if (resultType != SGM::ResultTypeOK)
        {
        std::cerr << rResult.Message() << std::endl;
        exit(1);
        }
    std::vector<SGM::Complex> *aComplexes = (std::vector<SGM::Complex> *) &aEntities;
    SGM::Complex ComplexID = aComplexes->front();
    if (aComplexes->size() > 1)
        {
        ComplexID = SGM::MergeComplexes(rResult, *aComplexes);
        }
    SGM::Complex CoverID = SGM::CoverComplex(rResult, ComplexID);
    std::vector<SGM::Complex> aParts;
    aParts.push_back(ComplexID);
    aParts.push_back(CoverID);
    SGM::Complex AnswerID=SGM::MergeComplexes(rResult, aParts);
    SGM::DeleteEntity(rResult,CoverID); 
    SGM::DeleteEntity(rResult,ComplexID); 
    std::string OutputSTLFile(argv[3]);

    SGM::SaveSTL(rResult, OutputSTLFile, AnswerID, Options);
    }

// Headlessly load each FILE via SGM::ReadFile (no Qt/GUI) and report OK/FAILED
// per file, mirroring the per-format options MainWindow/ModelData apply when
// opening a file interactively (e.g. STL triangle merging).
// Returns 0 if every file loaded without error, 1 otherwise.

int ValidateFiles(int argc, char **argv)
    {
    if (argc < 3)
        {
        std::cerr << "error: no files given to load" << std::endl;
        PrintUsage(GetExecutableName(argv[0]));
        return 1;
        }

    int nFailed = 0;
    int nTotal = argc - 2;

    for (int i = 2; i < argc; ++i)
        {
        std::string sFile(argv[i]);

        std::vector<SGM::Entity> aEntities;
        std::vector<std::string> aLog;
        SGM::TranslatorOptions Options;
        if (SGM::GetFileType(sFile) == SGM::STLFileType)
            {
            Options.m_bMerge = true;
            }

        bool bOK = false;
        bool bThrew = false;
        std::string sMessage;

        try
            {
            SGMInternal::thing *pThing = SGM::CreateThing();
            SGM::Result rResult(pThing);
            SGM::ReadFile(rResult, sFile, aEntities, aLog, Options);
            bOK = (rResult.GetResult() == SGM::ResultTypeOK);
            sMessage = rResult.Message();
            }
        catch (std::exception const &e)
            {
            bThrew = true;
            sMessage = e.what();
            }
        catch (...)
            {
            bThrew = true;
            sMessage = "unknown non-std::exception thrown";
            }

        if (!bOK)
            {
            ++nFailed;
            }

        std::cout << (bOK ? "OK      " : (bThrew ? "THREW   " : "FAILED  "))
                   << sFile << "  (entities=" << aEntities.size() << ")";
        if (!bOK)
            {
            std::cout << "  -- " << sMessage;
            }
        std::cout << std::endl;
        }

    std::cout << std::endl << (nTotal - nFailed) << "/" << nTotal
               << " files loaded OK";
    if (nFailed > 0)
        {
        std::cout << ", " << nFailed << " FAILED";
        }
    std::cout << std::endl;

    return nFailed > 0 ? 1 : 0;
    }

// return false if we are not done processing and should go on to the QtApp

bool RunFromCommandLine(int argc, char **argv)
    {
    std::string sExecutableName = GetExecutableName(argv[0]);

    // did the user supply arguments?
    if (argc == 1)
        {
        return false;
        }

    // Are there options present.
    if (0 == strncmp(argv[1], "-", 1))
        {
        // Is it help or version?
        for (int i = 1; i < argc; ++i)
            {
            if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
                {
                PrintHelp(sExecutableName);
                exit(0);
                }
            else
                {
                if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0)
                    {
                    PrintVersion();
                    exit(0);
                    }
                }
            }

        // Is the first one cover?
        if (strcmp(argv[1], "--cover") == 0 || strcmp(argv[1], "-c") == 0)
            {
            CoverSTL(argc, argv);
            }
        else if (strcmp(argv[1], "--fractures") == 0 || strcmp(argv[1], "-f") == 0)
            {
            FindFractures(argc, argv);
            }
        else if (strcmp(argv[1], "--load") == 0 || strcmp(argv[1], "-l") == 0)
            {
            exit(ValidateFiles(argc, argv));
            }
        else
            {
            // unrecognized options
            PrintUsage(sExecutableName);
            }
        // we are done processing
        return true;
        }

    else
        {
        // there are no options
        // there are file arguments but we will let the QtApp take care of it
        return false;
        }
    }

int main(int argc, char **argv)
    {

    // Run from command line arguments.

    if (RunFromCommandLine(argc,argv))
        {
        // we are finished
        exit(0);
        }

    // Setup some defaults for the settings file
    QApplication::setOrganizationName("Sandia");
    QApplication::setApplicationName("SGM-Model-Viewer");
    QSettings::setDefaultFormat(QSettings::IniFormat);

    // The surface format should be set before we instantiate the
    // QApplication
    QSurfaceFormat::setDefaultFormat(SGMGraphicsWidget::default_format());

    QApplication app(argc, argv);

    // Setup the main window
    MainWindow mw;
    mw.show();

#if defined (__linux__)
    QFont new_font = app.font();
    new_font.setPointSize( 14 ); //your option
    //new_font.setWeight( int ** ); //your option
    app.setFont( new_font );     // Run the application
#endif

    // Open any file arguments given on the command line (usage advertises
    // this; file_open_recent is private, so invoke it through the
    // meta-object system rather than widening the class's public interface).
    for (int i = 1; i < argc; ++i)
        {
        QMetaObject::invokeMethod(&mw, "file_open_recent",
                                   Q_ARG(QString, QString::fromUtf8(argv[i])));
        }

    return QApplication::exec();
    }

