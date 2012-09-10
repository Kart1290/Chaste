/*

Copyright (c) 2005-2012, University of Oxford.
All rights reserved.

University of Oxford means the Chancellor, Masters and Scholars of the
University of Oxford, having an administrative office at Wellington
Square, Oxford OX1 2JD, UK.

This file is part of Chaste.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
 * Neither the name of the University of Oxford nor the names of its
   contributors may be used to endorse or promote products derived from this
   software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "ExecutableSupport.hpp"

#include <iostream>
#include <sstream>
#include <sys/utsname.h> // For uname
#include <hdf5.h>
#include <parmetis.h>

#include <boost/foreach.hpp>
typedef std::pair<std::string, std::string> StringPair;

#include "CommandLineArguments.hpp"
#include "Exception.hpp"
#include "PetscTools.hpp"
#include "PetscException.hpp"
#include "Version.hpp"
#include "ChasteSerialization.hpp"

#ifdef CHASTE_VTK
#define _BACKWARD_BACKWARD_WARNING_H 1 //Cut out the strstream deprecated warning for now (gcc4.3)
#include <vtkVersion.h>
#endif

#ifdef CHASTE_CVODE
#include <sundials/sundials_config.h>
#endif
//#include <xsd/cxx/version.hxx>

FileFinder ExecutableSupport::mOutputDirectory;

void ExecutableSupport::SetOutputDirectory(const std::string& rOutputDirectory)
{
    if (FileFinder::IsAbsolutePath(rOutputDirectory))
    {
        mOutputDirectory.SetPath(rOutputDirectory, RelativeTo::Absolute);
    }
    else
    {
        mOutputDirectory.SetPath(rOutputDirectory, RelativeTo::ChasteTestOutput);
    }
}

void ExecutableSupport::InitializePetsc(int* pArgc, char*** pArgv)
{
    // Store the arguments in case other code needs them
    CommandLineArguments::Instance()->p_argc = pArgc;
    CommandLineArguments::Instance()->p_argv = pArgv;
    // Initialise PETSc
    PETSCEXCEPT(PetscInitialize(pArgc, pArgv, PETSC_NULL, PETSC_NULL));
    // Set default output folder
    if (!mOutputDirectory.IsPathSet())
    {
        mOutputDirectory.SetPath("", RelativeTo::ChasteTestOutput);
    }
}

void ExecutableSupport::ShowCopyright()
{
    // Compilation information
    std::stringstream provenance_msg;
    provenance_msg << "This version of Chaste was compiled on:\n";
    provenance_msg << ChasteBuildInfo::GetBuildTime() << " by " << ChasteBuildInfo::GetBuilderUnameInfo() << " (uname)\n";
    provenance_msg << "from revision number " << ChasteBuildInfo::GetRevisionNumber() << " with build type " << ChasteBuildInfo::GetBuildInformation() << ".\n\n";

    // Only show one copy of copyright/header
    if (PetscTools::AmMaster())
    {
        std::cout << ChasteBuildInfo::GetLicenceText() << std::endl;

        // Write provenance information to stdout
        std::cout << provenance_msg.str() << std::flush;
    }
}

void ExecutableSupport::ShowParallelLaunching()
{
    if (PetscTools::IsParallel())
    {
        // Information to show that Chaste is being run in parallel
        PetscTools::BeginRoundRobin();
        std::cout << "Chaste launched on process " << PetscTools::GetMyRank()
            << " of " << PetscTools::GetNumProcs() << "." << std::endl << std::flush;
        PetscTools::EndRoundRobin();
    }
}

void ExecutableSupport::WriteMachineInfoFile(std::string fileBaseName)
{
    if (!mOutputDirectory.IsPathSet()) mOutputDirectory.SetPath("", RelativeTo::ChasteTestOutput);
    OutputFileHandler out_file_handler(mOutputDirectory, false);
    std::stringstream file_name;
    file_name << fileBaseName << "_" << PetscTools::GetMyRank() << ".txt";
    out_stream out_file = out_file_handler.OpenOutputFile(file_name.str());
    *out_file << "Process " << PetscTools::GetMyRank() << " of "
        << PetscTools::GetNumProcs() << "." << std::endl << std::flush;

    struct utsname uts_info;
    uname(&uts_info);

    *out_file << "uname sysname  = " << uts_info.sysname << std::endl << std::flush;
    *out_file << "uname nodename = " << uts_info.nodename << std::endl << std::flush;
    *out_file << "uname release  = " << uts_info.release << std::endl << std::flush;
    *out_file << "uname version  = " << uts_info.version << std::endl << std::flush;
    *out_file << "uname machine  = " << uts_info.machine << std::endl << std::flush;
    char buffer[100];
    FILE * system_info;

    *out_file << "\nInformation on number and type of processors:\n";
    system_info = popen("grep ^model.name /proc/cpuinfo", "r");
    while ( fgets(buffer, 100, system_info) != NULL )
    {
        *out_file << buffer;
    }
    fclose(system_info);

    *out_file << "\nInformation on processor caches, in the same order as above:\n";
    system_info = popen("grep ^cache.size /proc/cpuinfo", "r");
    while ( fgets(buffer, 100, system_info) != NULL )
    {
        *out_file << buffer;
    }
    fclose(system_info);

    *out_file << "\nInformation on system memory:\n";
    system_info = popen("grep ^MemTotal /proc/meminfo", "r");
    while ( fgets(buffer, 100, system_info) != NULL )
    {
        *out_file << buffer;
    }
    fclose(system_info);

    out_file->close();
}

void ExecutableSupport::WriteProvenanceInfoFile()
{
    if (!mOutputDirectory.IsPathSet()) mOutputDirectory.SetPath("", RelativeTo::ChasteTestOutput);
    OutputFileHandler out_file_handler(mOutputDirectory, false);
    out_stream out_file = out_file_handler.OpenOutputFile("provenance_info_", PetscTools::GetMyRank(), ".txt");

    // Compilation information
    std::stringstream provenance_msg;
    provenance_msg << "This version of Chaste was compiled on:\n";
    provenance_msg << ChasteBuildInfo::GetBuildTime() << " by " << ChasteBuildInfo::GetBuilderUnameInfo() << " (uname)\n";
    provenance_msg << "from revision number " << ChasteBuildInfo::GetRevisionNumber() << " with build type " << ChasteBuildInfo::GetBuildInformation() << ".\n\n";
    *out_file << provenance_msg.str();

    std::string output;
    GetBuildInfo(output);
    *out_file << output;

    out_file->close();
}

void ExecutableSupport::GetBuildInfo(std::string& rInfo)
{
    std::stringstream output;
    output << "<ChasteBuildInfo>\n";

    output << "\t<ProvenanceInfo>\n";
    output << "\t\t<VersionString>"<< ChasteBuildInfo::GetVersionString() << "</VersionString> <!-- build specific -->\n";
    output << "\t\t<IsWorkingCopyModified>"<< ChasteBuildInfo::IsWorkingCopyModified() << "</IsWorkingCopyModified>\n";
    output << "\t\t<BuildInformation>"<< ChasteBuildInfo::GetBuildInformation() << "</BuildInformation>\n";
    output << "\t\t<BuildTime>"<< ChasteBuildInfo::GetBuildTime() << "</BuildTime>\n";
    output << "\t\t<CurrentTime>"<< ChasteBuildInfo::GetCurrentTime() << "</CurrentTime>\n";
    output << "\t\t<BuilderUnameInfo>"<< ChasteBuildInfo::GetBuilderUnameInfo() << "</BuilderUnameInfo>\n";
    output << "\t\t<Projects>\n";
    BOOST_FOREACH(const StringPair& r_project_version, ChasteBuildInfo::rGetProjectVersions())
    {
#define COVERAGE_IGNORE
        // No projects are checked out for continuous builds normally!
        output<< "\t\t\t<Name>" << r_project_version.first << "</Name><Version>"
                 << r_project_version.second << "</Version>\n";
#undef COVERAGE_IGNORE
    }
    output << "\t\t</Projects>\n";
    output << "\t</ProvenanceInfo>\n";

    output << "\t<Compiler>\n";
    output << "\t\t<NameAndVersion>" << ChasteBuildInfo::GetCompilerType() << ", version " << ChasteBuildInfo::GetCompilerVersion() << "</NameAndVersion>\n" ;
    output << "\t\t<Flags>" << ChasteBuildInfo::GetCompilerFlags() << "</Flags>\n" ;
    output << "\t</Compiler>\n";

    output << "\t<Libraries>\n";

    output << "\t\t<CompiledIn>\n";
    output << "\t\t\t<PETSc>" << PETSC_VERSION_MAJOR << "." << PETSC_VERSION_MINOR << "." << PETSC_VERSION_SUBMINOR << "</PETSc>\n";
    output << "\t\t\t<Boost>" << BOOST_VERSION  / 100000 << "." << BOOST_VERSION / 100 % 1000 << "." << BOOST_VERSION % 100 << "</Boost>\n";
    output << "\t\t\t<HDF5>" << H5_VERS_MAJOR <<  "." << H5_VERS_MINOR << "." << H5_VERS_RELEASE << "</HDF5>\n";
    output << "\t\t\t<Parmetis>" << PARMETIS_MAJOR_VERSION << "." << PARMETIS_MINOR_VERSION;
#ifdef PARMETIS_SUBMINOR_VERSION // they only added this in v4.? !!
    output << "." << PARMETIS_SUBMINOR_VERSION;
#endif
    output << "</Parmetis>" << std::endl;
    output << "\t\t</CompiledIn>\n";

    output << "\t\t<Binaries>\n";
    output << "\t\t\t<XSD>" <<  ChasteBuildInfo::GetXsdVersion() << "</XSD>\n";
    output << "\t\t</Binaries>\n";

    output << "\t\t<Optional>\n";
#ifdef CHASTE_VTK
    output << "\t\t\t<VTK>" << VTK_MAJOR_VERSION << "." << VTK_MINOR_VERSION << "</VTK>\n";
#else
    output << "\t\t\t<VTK>no</VTK>\n";
#endif

#ifdef CHASTE_CVODE
    output << "\t\t\t<SUNDIALS>" << SUNDIALS_PACKAGE_VERSION << "</SUNDIALS> <!-- includes Cvode of a different version number --> \n";
#else
    output << "\t\t\t<SUNDIALS>no</SUNDIALS>\n";
#endif

#ifdef CHASTE_ADAPTIVITY
    output << "\t\t\t<Adaptivity>yes</Adaptivity>\n";
#else
    output << "\t\t\t<Adaptivity>no</Adaptivity>\n";
#endif
    output << "\t\t</Optional>\n";

    output << "\t</Libraries>\n";

    output << "</ChasteBuildInfo>" << std::endl;
    rInfo = output.str();
}

void ExecutableSupport::StandardStartup(int* pArgc, char*** pArgv)
{
    InitializePetsc(pArgc, pArgv);
    ShowCopyright();
    ShowParallelLaunching();
}

void ExecutableSupport::PrintError(const std::string& rMessage, bool masterOnly)
{
    if (!masterOnly || PetscTools::AmMaster())
    {
        // Write the error message to stderr
        std::cerr << rMessage << std::endl;
    }

    // Write the error message to file
    if (!mOutputDirectory.IsPathSet()) mOutputDirectory.SetPath("", RelativeTo::ChasteTestOutput);
    OutputFileHandler out_file_handler(mOutputDirectory, false);
    out_stream out_file = out_file_handler.OpenOutputFile("chaste_errors_", PetscTools::GetMyRank(), ".txt", std::ios::out | std::ios::app);
    *out_file << rMessage << std::endl;
    out_file->close();
}

void ExecutableSupport::Print(const std::string& rMessage)
{
    if (PetscTools::AmMaster())
    {
        // Write the error message to stdout
        std::cout << rMessage << std::endl << std::flush;
    }
}

void ExecutableSupport::FinalizePetsc()
{
    // Causes memory failure (and seg fault) in PETSc 3.2 with MPICH-1
    PetscFinalize();
}
