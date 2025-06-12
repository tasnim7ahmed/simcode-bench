#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/buildings-module.h"
#include "ns3/antenna-module.h"
#include "ns3/config-store-module.h"
#include "ns3/log.h"
#include <fstream>
#include <cmath>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VehicularEnvironmentSimulation");

std::ofstream g_logFile;
double g_cumulativeSnr = 0.0;
double g_cumulativePathloss = 0.0;
uint32_t g_sampleCount = 0;

/**
 * Compute pathloss using urban macro pathloss model from 3GPP TR 38.901
 * This is a simplyfied rural/urban macro pathloss (LOS) for demonstration.
 * For in-depth usage, integration with full ns-3 Models/BuildingsPathlossModel is preferred.
 */
double
ComputePathloss(const Vector& txPos, const Vector& rxPos, double freqHz)
{
    double d3d = CalculateDistance(txPos, rxPos);
    double fcGHz = freqHz / 1e9;
    if (d3d < 10.0) d3d = 10.0; // Minimum distance
    double pl = 32.4 + 21.0 * std::log10(d3d) + 20.0 * std::log10(fcGHz); // 3GPP UMa-LOS
    return pl;
}

/**
 * Simulate SNR given TX and RX antenna gains, TX power, pathloss, and noise figure
 */
double
ComputeSnr(double txPowerDbm, double pathlossDb, double txGainDb, double rxGainDb, double noiseFigureDb)
{
    double noiseFloorDbm = -174.0 + 10 * std::log10(20e6) + noiseFigureDb; // 20 MHz
    double rxPowerDbm = txPowerDbm + txGainDb + rxGainDb - pathlossDb;
    return rxPowerDbm - noiseFloorDbm;
}

/**
 * Simple DFT-based beamforming gain estimation for uniform planar array.
 * Ntx, Nty = UPA rows and columns.
 * For demonstration: returns mainlobe gain (10*log10(N))
 */
double
CalculateDftBfGain(uint32_t ntx, uint32_t nty)
{
    uint32_t n = ntx * nty;
    return 10 * std::log10(static_cast<double>(n));
}

void
SampleMetrics(Ptr<Node> txNode, Ptr<Node> rxNode, Ptr<MobilityModel> txMob, Ptr<MobilityModel> rxMob,
              Ptr<AntennaModel> txAnt, Ptr<AntennaModel> rxAnt, double frequencyHz,
              double txPowerDbm, double noiseFigureDb, Time t)
{
    double pathloss = ComputePathloss(txMob->GetPosition(), rxMob->GetPosition(), frequencyHz);

    // Antenna array size and DFT beamforming gain
    UniformPlanarArray* txUpa = dynamic_cast<UniformPlanarArray*>(PeekPointer(txAnt));
    UniformPlanarArray* rxUpa = dynamic_cast<UniformPlanarArray*>(PeekPointer(rxAnt));
    double txGain = (txUpa) ? CalculateDftBfGain(txUpa->GetNumRows(), txUpa->GetNumColumns()) : 0.0;
    double rxGain = (rxUpa) ? CalculateDftBfGain(rxUpa->GetNumRows(), rxUpa->GetNumColumns()) : 0.0;

    double snr = ComputeSnr(txPowerDbm, pathloss, txGain, rxGain, noiseFigureDb);

    g_cumulativeSnr += snr;
    g_cumulativePathloss += pathloss;
    g_sampleCount++;

    g_logFile << t.GetSeconds()
              << " " << snr
              << " " << pathloss
              << std::endl;

    Simulator::Schedule(Seconds(0.1), &SampleMetrics, txNode, rxNode, txMob, rxMob, txAnt, rxAnt, frequencyHz, txPowerDbm, noiseFigureDb, t + Seconds(0.1));
}

int main(int argc, char** argv)
{
    // Simulation parameters
    uint32_t numVehicles = 5;
    double simTime = 10.0;
    double frequencyHz = 3.5e9;
    double txPowerDbm = 23.0;
    double noiseFigureDb = 7.0;
    uint32_t upaRows = 4;
    uint32_t upaCols = 4;
    std::string scenario = "urban"; // "urban" or "highway"

    CommandLine cmd;
    cmd.AddValue("numVehicles", "Number of vehicles", numVehicles);
    cmd.AddValue("simTime", "Simulation time (s)", simTime);
    cmd.AddValue("scenario", "urban|highway", scenario);
    cmd.Parse(argc, argv);

    // Create nodes (vehicles)
    NodeContainer vehicles;
    vehicles.Create(numVehicles);

    // Set up mobility
    MobilityHelper mobility;
    if (scenario == "urban") {
        // Urban grid-like movement
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
            "MinX", DoubleValue(0.0),
            "MinY", DoubleValue(0.0),
            "DeltaX", DoubleValue(25.0),
            "DeltaY", DoubleValue(25.0),
            "GridWidth", UintegerValue(3),
            "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
            "Bounds", RectangleValue(Rectangle(-50, 250, -50, 250)),
            "Distance", DoubleValue(15.0),
            "Speed", StringValue("ns3::ConstantRandomVariable[Constant=8.0]"));
    } else { // highway
        Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
        for (uint32_t i = 0; i < numVehicles; ++i) {
            posAlloc->Add(Vector(i*60.0, 0, 1.5)); // Along x, at 1.5m height
        }
        mobility.SetPositionAllocator(posAlloc);
        mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    }
    mobility.Install(vehicles);

    if (scenario == "highway") {
        for (uint32_t i = 0; i < numVehicles; ++i) {
            Ptr<ConstantVelocityMobilityModel> cvm = vehicles.Get(i)->GetObject<ConstantVelocityMobilityModel>();
            cvm->SetVelocity(Vector(30.0, 0.0, 0.0)); // 108 km/h
        }
    }

    // Install Antennas (UPA)
    std::vector<Ptr<AntennaModel>> vehicleAntennas;
    for (uint32_t i = 0; i < numVehicles; ++i) {
        Ptr<UniformPlanarArray> upa = CreateObject<UniformPlanarArray>();
        upa->SetAttribute("Rows", UintegerValue(upaRows));
        upa->SetAttribute("Columns", UintegerValue(upaCols));
        upa->SetAttribute("ElementSpacing", DoubleValue(0.5)); // In wavelengths
        vehicleAntennas.push_back(upa);
    }

    // Output file
    g_logFile.open("vehicular_metrics.log");
    g_logFile << "# time(s) SNR(dB) Pathloss(dB)" << std::endl;

    // For demonstration, sample first and second vehicles' link
    Ptr<Node> txNode = vehicles.Get(0);
    Ptr<Node> rxNode = vehicles.Get(1);
    Ptr<MobilityModel> txMob = txNode->GetObject<MobilityModel>();
    Ptr<MobilityModel> rxMob = rxNode->GetObject<MobilityModel>();
    Ptr<AntennaModel> txAnt = vehicleAntennas[0];
    Ptr<AntennaModel> rxAnt = vehicleAntennas[1];

    Simulator::Schedule(Seconds(0.0), &SampleMetrics, txNode, rxNode, txMob, rxMob, txAnt, rxAnt, frequencyHz, txPowerDbm, noiseFigureDb, Seconds(0.0));
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    g_logFile.close();

    std::cout << "Simulation finished. Average SNR (dB): "
              << (g_sampleCount ? g_cumulativeSnr / g_sampleCount : 0)
              << ", Average Pathloss (dB): "
              << (g_sampleCount ? g_cumulativePathloss / g_sampleCount : 0)
              << std::endl;
    std::cout << "Metrics log written to vehicular_metrics.log" << std::endl;

    return 0;
}