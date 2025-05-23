#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h" // For ThreeGppChannelModel, UpaAntennaModel
#include "ns3/spectrum-module.h" // For SpectrumPropagationLossModel

#include <fstream>
#include <iomanip> // For std::fixed and std::setprecision
#include <cmath>   // For std::log10, std::pow

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VehicularThreeGppSimulation");

// Global variables for simulation parameters
static bool g_isUrbanScenario = true; // true for Urban, false for Highway
static uint32_t g_numVehicles = 5;
static double g_simulationTime = 10.0; // seconds
static double g_channelUpdatePeriod = 0.1; // seconds, frequency of logging metrics
static double g_txPower = 20.0; // dBm (e.g., 100mW)
static double g_frequency = 3.5e9; // 3.5 GHz
static double g_rxNoiseFigure = 5.0; // dB
static std::string g_outputFileName = "vehicular_metrics.csv";

// Output file stream
std::ofstream g_outputFile;

/**
 * \brief Calculates and logs average SNR and Pathloss for all vehicle pairs.
 * \param channelModel Pointer to the ThreeGppChannelModel instance.
 * \param vehicles NodeContainer holding all vehicle nodes.
 *
 * This function iterates through all unique pairs of vehicles, calculates the
 * pathloss and SNR between them, and logs the average values to a CSV file.
 * It then reschedules itself to run again after g_channelUpdatePeriod.
 */
void LogMetrics(Ptr<ThreeGppChannelModel> channelModel, NodeContainer vehicles)
{
    double totalPathloss = 0.0;
    double totalSnr = 0.0;
    uint32_t numPairs = 0;

    // Calculate noise power: Noise Floor + 10*log10(Bandwidth) + Receiver Noise Figure
    // A typical bandwidth for NR is 20 MHz.
    double noiseFloor_dBm_per_Hz = -174.0; // Thermal noise power spectral density (kT)
    double bandwidth_Hz = 20e6; // Example bandwidth for noise calculation
    double noisePower_dBm = noiseFloor_dBm_per_Hz + 10 * std::log10(bandwidth_Hz) + g_rxNoiseFigure;

    // Iterate over all unique pairs of vehicles
    for (uint32_t i = 0; i < vehicles.GetN(); ++i)
    {
        for (uint32_t j = i + 1; j < vehicles.GetN(); ++j) // Consider unique pairs (i, j) and not (j, i)
        {
            Ptr<Node> txNode = vehicles.Get(i);
            Ptr<Node> rxNode = vehicles.Get(j);

            Ptr<MobilityModel> txMobility = txNode->GetObject<MobilityModel>();
            Ptr<MobilityModel> rxMobility = rxNode->GetObject<MobilityModel>();

            NS_ASSERT_MSG(txMobility != nullptr && rxMobility != nullptr, "MobilityModel not found on nodes.");

            // Get current positions
            Vector txPos = txMobility->GetPosition();
            Vector rxPos = rxMobility->GetPosition();

            // Calculate pathloss for the current positions using the 3GPP channel model
            double currentPathloss = channelModel->GetLoss(txPos, rxPos);
            totalPathloss += currentPathloss;

            // Calculate SNR: RxPower (linear) / NoisePower (linear)
            // RxPower_dBm = TxPower_dBm - Pathloss_dB
            double rxPower_dBm = g_txPower - currentPathloss;
            double rxPower_mW = std::pow(10.0, rxPower_dBm / 10.0);
            double noisePower_mW = std::pow(10.0, noisePower_dBm / 10.0);

            double currentSnr_linear = rxPower_mW / noisePower_mW;
            double currentSnr_dB = 10.0 * std::log10(currentSnr_linear);
            totalSnr += currentSnr_dB;

            numPairs++;
        }
    }

    if (numPairs > 0)
    {
        double avgPathloss = totalPathloss / numPairs;
        double avgSnr = totalSnr / numPairs;

        // Write current simulation time, average pathloss, and average SNR to the output file
        g_outputFile << Simulator::Now().GetSeconds() << ","
                     << avgPathloss << ","
                     << avgSnr << std::endl;

        NS_LOG_INFO("Time: " << Simulator::Now().GetSeconds() << "s, Avg Pathloss: " << avgPathloss << " dB, Avg SNR: " << avgSnr << " dB");
    }

    // Schedule the next logging event if simulation time is not yet reached
    if (Simulator::Now().GetSeconds() < g_simulationTime)
    {
        Simulator::Schedule(Seconds(g_channelUpdatePeriod), &LogMetrics, channelModel, vehicles);
    }
}


int main(int argc, char *argv[])
{
    // Configure command line arguments
    CommandLine cmd(__FILE__);
    cmd.AddValue("urban", "Set to true for urban scenario, false for highway.", g_isUrbanScenario);
    cmd.AddValue("numVehicles", "Number of vehicles in the simulation.", g_numVehicles);
    cmd.AddValue("simTime", "Total simulation time in seconds.", g_simulationTime);
    cmd.AddValue("outputFile", "Name of the output CSV file.", g_outputFileName);
    cmd.Parse(argc, argv);

    // Configure logging for the simulation component
    LogComponentEnable("VehicularThreeGppSimulation", LOG_LEVEL_INFO);

    // Open the output file for writing metrics
    g_outputFile.open(g_outputFileName, std::ios_base::out | std::ios_base::trunc);
    if (!g_outputFile.is_open())
    {
        NS_LOG_ERROR("Failed to open output file: " << g_outputFileName);
        return 1;
    }
    // Write CSV header and set precision for floating point numbers
    g_outputFile << "Time_s,AvgPathloss_dB,AvgSNR_dB" << std::endl;
    g_outputFile << std::fixed << std::setprecision(6);

    // Create nodes to represent vehicles
    NodeContainer vehicles;
    vehicles.Create(g_numVehicles);

    // Setup Mobility Models for vehicles
    MobilityHelper mobility;
    if (g_isUrbanScenario)
    {
        NS_LOG_INFO("Configuring Urban Scenario Mobility (WaypointMobilityModel).");
        // Urban scenario: Vehicles move with varying speeds and turns, simulating city streets.
        // Using WaypointMobilityModel to define specific paths for vehicles.
        for (uint32_t i = 0; i < g_numVehicles; ++i)
        {
            Ptr<WaypointMobilityModel> wpm = CreateObject<WaypointMobilityModel>();
            vehicles.Get(i)->AggregateObject(wpm);

            // Initial position for each vehicle
            wpm->SetPosition(Vector(i * 15.0, i * 10.0, 1.5)); // Spread them out initially

            // Define a simple square-like movement pattern for each vehicle
            // The times are relative to the start of the waypoint sequence for each vehicle.
            wpm->AddWaypoint(Waypoint(Seconds(1.0), Vector(i * 15.0 + 20.0, i * 10.0, 1.5))); // Move right
            wpm->AddWaypoint(Waypoint(Seconds(2.0), Vector(i * 15.0 + 20.0, i * 10.0 + 20.0, 1.5))); // Move up
            wpm->AddWaypoint(Waypoint(Seconds(3.0), Vector(i * 15.0, i * 10.0 + 20.0, 1.5))); // Move left
            wpm->AddWaypoint(Waypoint(Seconds(4.0), Vector(i * 15.0, i * 10.0, 1.5))); // Move down

            // Repeat the pattern to cover the simulation duration
            for (double t = 5.0; t < g_simulationTime; t += 4.0)
            {
                wpm->AddWaypoint(Waypoint(Seconds(t + 1.0), Vector(i * 15.0 + 20.0, i * 10.0, 1.5)));
                wpm->AddWaypoint(Waypoint(Seconds(t + 2.0), Vector(i * 15.0 + 20.0, i * 10.0 + 20.0, 1.5)));
                wpm->AddWaypoint(Waypoint(Seconds(t + 3.0), Vector(i * 15.0, i * 10.0 + 20.0, 1.5)));
                wpm->AddWaypoint(Waypoint(Seconds(t + 4.0), Vector(i * 15.0, i * 10.0, 1.5)));
            }
        }
    }
    else
    {
        NS_LOG_INFO("Configuring Highway Scenario Mobility (ConstantVelocityMobilityModel).");
        // Highway scenario: Vehicles move primarily in a straight line with constant or slightly varying velocity.
        mobility.SetMobilityModelType("ns3::ConstantVelocityMobilityModel");
        Ptr<UniformRandomVariable> xPosRand = CreateObject<UniformRandomVariable>();
        xPosRand->SetAttribute("Min", DoubleValue(0.0));
        xPosRand->SetAttribute("Max", DoubleValue(100.0)); // Initial X position spread
        Ptr<UniformRandomVariable> yPosRand = CreateObject<UniformRandomVariable>();
        yPosRand->SetAttribute("Min", DoubleValue(0.0));
        yPosRand->SetAttribute("Max", DoubleValue(10.0)); // Initial Y position spread for different lanes
        Ptr<UniformRandomVariable> speedRand = CreateObject<UniformRandomVariable>();
        speedRand->SetAttribute("Min", DoubleValue(25.0)); // m/s (90 km/h)
        speedRand->SetAttribute("Max", DoubleValue(35.0)); // m/s (126 km/h)

        for (uint32_t i = 0; i < g_numVehicles; ++i)
        {
            Ptr<ConstantVelocityMobilityModel> cvmm = CreateObject<ConstantVelocityMobilityModel>();
            vehicles.Get(i)->AggregateObject(cvmm);
            // Set initial position and velocity for each vehicle
            cvmm->SetPosition(Vector(xPosRand->GetValue(), yPosRand->GetValue(), 1.5));
            cvmm->SetVelocity(Vector(speedRand->GetValue(), 0.0, 0.0)); // Move along X-axis
        }
    }

    // Set up Antenna Model (Uniform Planar Array - UPA)
    // A UPA is used to model antenna arrays that support beamforming.
    Ptr<UpaAntennaModel> upaAntenna = CreateObject<UpaAntennaModel>();
    upaAntenna->SetNumRows(4); // 4 rows of antenna elements
    upaAntenna->SetNumColumns(4); // 4 columns of antenna elements
    upaAntenna->SetElementSpacing(0.5); // Element spacing in wavelengths (lambda/2)

    // Set up 3GPP Channel Model (TR 37.885 / TR 38.901 standards)
    ThreeGppChannelModelHelper channelHelper;
    channelHelper.SetFrequency(g_frequency); // Set carrier frequency
    channelHelper.SetAntenna(upaAntenna); // Assign the UPA antenna model for spatial domain modeling

    // Select the 3GPP propagation scenario based on the simulation type
    if (g_isUrbanScenario)
    {
        // UMi_StreetCanyon is suitable for urban vehicular environments, considering building effects.
        channelHelper.SetScenario(ThreeGppChannelModelHelper::Scenario::UMi_StreetCanyon);
    }
    else
    {
        // RMa_LOS (Rural Macro-cell Line-of-Sight) is suitable for open highway environments.
        channelHelper.SetScenario(ThreeGppChannelModelHelper::Scenario::RMa_LOS);
    }

    // Install the 3GPP channel model.
    // The channel model instance is responsible for calculating pathloss and fading,
    // incorporating the antenna patterns (beamforming gain) as specified by the UPA model
    // and the chosen 3GPP scenario. The "DFT methods" for beamforming are implicitly
    // supported by the UPA model's ability to form beams and the channel's
    // calculation of spatial gains based on antenna patterns and angles of arrival/departure.
    Ptr<ThreeGppChannelModel> channel = channelHelper.Create();

    // Schedule the initial logging event
    Simulator::Schedule(Seconds(0.0), &LogMetrics, channel, vehicles);

    // Run the simulation
    Simulator::Stop(Seconds(g_simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    // Close the output file after simulation ends
    g_outputFile.close();

    NS_LOG_INFO("Simulation finished. Metrics saved to " << g_outputFileName);

    return 0;
}