#include <OpenVolumeMesh/IO/ovmb_write.hh>
#include <HexHex/Utils/FileAccessor.hh>
#include <iostream>
#include <fstream>
#include <filesystem>

#include <HexHex/HexHex.hh>
#include <nlohmann/json.hpp>
#include <HexHex/Utils/Stopwatches.hh>
#include <HexHex/Utils/Utils.hh>
#include <libTimekeeper/StopWatchPrinting.hh>
#include <CLI/CLI.hpp>

// Include the file manager header
#include <OpenVolumeMesh/FileManager/FileManager.hh>
// Include the polyhedral mesh header
#include <OpenVolumeMesh/Mesh/PolyhedralMesh.hh>

namespace OVM = OpenVolumeMesh;

struct Options {
    std::filesystem::path inTetFile;
    std::filesystem::path outHexFile;
    std::optional<std::filesystem::path> outPWLFile;
    std::optional<std::filesystem::path> outReportFile;
    std::optional<std::filesystem::path> inConfigFile;
    std::optional<int> igm_scaling_factor;
    std::optional<int> num_threads;
};

int main(int argc, char* argv[])
{
    CLI::App app{"HexHex: Highspeed Extraction of Hexahedral Meshes"};
    argv = app.ensure_utf8(argv);

    Options options;
    app.add_option("-i, --in", options.inTetFile, "Input file (.ovmb, .ovm, .hexex)")->required();
    app.add_option("-o, --out-hex", options.outHexFile, "Output file (.ovmb, .ovm, .mesh)")->required();
    app.add_option("--out-pwl", options.outPWLFile, "Output file for piecewise linear mesh");
    app.add_option("--report", options.outReportFile, "Output file with details about the extraction process (.json)");
    app.add_option("--config", options.inConfigFile, "Config file (.json). Used when parameters are not explicitly set.");
    app.add_option("--scale", options.igm_scaling_factor, "Parametrization scaling factor (positive integer)")->check(CLI::PositiveNumber);
    app.add_option("--nthreads", options.num_threads, "Number of threads or nonpositive to use number of available cores");

    CLI11_PARSE(app, argc, argv);

    HexHex::Config config;
    if (options.inConfigFile)
    {
        HexHex::loadConfig(options.inConfigFile.value(), config);
    }
    if (options.outPWLFile) {
        config.extract_piecewise_linear_faces = true;
        config.extract_piecewise_linear_edges = true;
    }
    if (options.igm_scaling_factor.has_value()) {
        config.igm_scaling_factor = options.igm_scaling_factor.value();
    }
    if (options.num_threads.has_value()) {
        config.num_threads = options.num_threads.value();
    }

    std::cout << "Load Input Tet Mesh from " << options.inTetFile << std::endl;
    auto maybe_inputmesh = HexHex::loadInputFromFile(options.inTetFile);
    if (!maybe_inputmesh.has_value()) {
        std::cerr << "Failed to load input mesh " << options.inTetFile << std::endl;
        return 1;
    }
    auto res = HexHex::extractHexMesh(maybe_inputmesh->mesh, maybe_inputmesh->igm, config);

    // Hex Mesh
    if (res.hex_mesh != nullptr) {
        std::cout << "Save HexHex Hex Mesh to " << options.outHexFile << std::endl;
        HexHex::saveOutputToFile(options.outHexFile, *res.hex_mesh);
    } else {
        std::cerr << "Hex extraction failed!" << std::endl;
    }

    // Piecewise Linear Mesh
    if (res.piecewise_linear_mesh != nullptr) {
        std::cout << "Save HexHex Piecewise Linear Mesh to " << *options.outPWLFile << std::endl;

        if (const std::string ext = options.outPWLFile->extension().string();
            ext == "ovmb")
            OpenVolumeMesh::IO::ovmb_write(*options.outPWLFile, *res.piecewise_linear_mesh);
        else if (ext == "ovm") {
            OpenVolumeMesh::IO::FileManager fileManager;
            fileManager.writeFile(*options.outPWLFile, *res.piecewise_linear_mesh);
        }
        // {
        //
        //     std::ifstream in(*options.outPWLFile);
        //     std::vector<double> points;
        //     std::vector<std::pair<int, int>> half_edges;
        //     std::vector<std::vector<int>> facets;
        //
        //     std::string line;
        //     while (std::getline(in, line)) {
        //         if (line.empty() || line[0] == '#')
        //             continue;
        //
        //         std::string prefix;
        //         {
        //             std::istringstream iss(line);
        //             iss >> prefix;
        //         }
        //
        //         if (prefix == "Vertices") {
        //             int vertices_nb;
        //             {
        //                 std::getline(in, line);
        //                 std::istringstream iss(line);
        //                 iss >> vertices_nb;
        //             }
        //             std::cout << "in vertices_nb: " << vertices_nb << std::endl;
        //
        //             for (int v = 0; v < vertices_nb; ++v) {
        //                 double x, y, z;
        //                 std::getline(in, line);
        //                 std::istringstream iss(line);
        //                 iss >> x >> y >> z;
        //                 points.push_back(x); points.push_back(y); points.push_back(z);
        //             }
        //         }
        //         else if (prefix == "Edges") {
        //             int edges_nb;
        //             {
        //                 std::getline(in, line);
        //                 std::istringstream iss(line);
        //                 iss >> edges_nb;
        //             }
        //             std::cout << "in edges_nb: " << edges_nb << std::endl;
        //
        //             for (int v = 0; v < edges_nb; ++v) {
        //                 int ev0, ev1;
        //                 std::getline(in, line);
        //                 std::istringstream iss(line);
        //                 iss >> ev0 >> ev1;
        //                 half_edges.emplace_back(ev0, ev1);
        //             }
        //         }
        //         else if (prefix == "Faces") {
        //             int faces_nb;
        //             {
        //                 std::getline(in, line);
        //                 std::istringstream iss(line);
        //                 iss >> faces_nb;
        //             }
        //             std::cout << "in faces_nb: " << faces_nb << std::endl;
        //
        //             for (int f = 0; f < faces_nb; ++f) {
        //                 std::getline(in, line);
        //                 std::istringstream iss(line);
        //
        //                 int face_vertices_nb;
        //                 iss >> face_vertices_nb;
        //
        //                 std::vector<int> cur_face_vertices;
        //                 for (int e = 0; e < face_vertices_nb; ++e) {
        //                     int cur_e;
        //                     iss >> cur_e;
        //                     cur_face_vertices.push_back(cur_e);
        //                 }
        //
        //                 facets.push_back(cur_face_vertices);
        //             }
        //         }
        //     }
        //
        //     in.close();
        //
        //     std::cout << "vertices nb: " << points.size()/3 << std::endl;
        //     std::cout << "edges nb: " << half_edges.size() << std::endl;
        //     std::cout << "facets nb: " << facets.size() << std::endl;
        //
        //     std::ofstream out("result.obj");
        //     for (int v = 0; v < points.size(); v += 3)
        //         out << "v " << points[v] << " " << points[v+1] << " " << points[v+2] << std::endl;
        //     // for (int e = 0; e < half_edges.size(); ++e) {
        //     //     if (half_edges[e].first < points.size()/3 && half_edges[e].second < points.size()/3)
        //     //         out << "l " << half_edges[e].first+1 << " " << half_edges[e].second+1 << std::endl;
        //     // }
        //
        //     for (int f = 0; f < facets.size(); ++f) {
        //         std::vector<int> fvs;
        //         for (int e = 0; e < facets[f].size(); ++e) {
        //             const auto& he = facets[f][e];
        //             if (he%2 == 0)
        //                 fvs.push_back(half_edges[he/2].first);
        //             else
        //                 fvs.push_back(half_edges[(he-1)/2].second);
        //         }
        //
        //         for (int j = 1; j < fvs.size()-1; ++j)
        //             out << "f " << fvs[0]+1 << " " << fvs[j]+1 << " " << fvs[j+1]+1 << std::endl;
        //     }
        //     out.close();
        // }
    } else if (options.outPWLFile) {
        std::cerr << "Piecewise-linear extraction failed!" << std::endl;
    }

    // Report
    if (options.outReportFile)
    {
        std::cout << "Save HexHex Report to " << *options.outReportFile << std::endl;
        nlohmann::json j = res.report;
        j["tet_mesh_filename"] = options.inTetFile.string();
        j["hex_mesh_filename"] = options.outHexFile.string();
        std::ofstream o(*options.outReportFile);
        o << std::setw(4) << j << std::endl;
    }

    return 0;
}
