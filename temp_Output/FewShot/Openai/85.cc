#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/antenna-module.h"
#include "ns3/propagation-module.h"
#include "ns3/buildings-module.h"
#include "ns3/config-store-module.h"
#include "ns3/df-antenna-array-model.h"
#include "ns3/three-gpp-channel-model.h"
#include "ns3/three-gpp-propagation-loss-model.h"
#include <fstream>
#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Vehicular3gppSimulation");

// Global variables for statistics
double g_totalSnr = 0.0;
double g_totalPathloss = 0.0;
uint32_t g_samples = 0;
std::ofstream g_logFile;

static void
LogSnrPathloss(Ptr<Node> txNode, Ptr<Node> rxNode, Ptr<ThreeGppPropagationLossModel> propLoss, Ptr<ThreeGppChannelModel> channelModel, uint32_t txAntennaPorts, uint32_t rxAntennaPorts)
{
    Vector txPos = txNode->GetObject<MobilityModel>()->GetPosition();
    Vector rxPos = rxNode->GetObject<MobilityModel>()->GetPosition();
    double pathloss = propLoss->CalcRxPower(0, txNode, rxNode, 0, 0);
    double noisePowerDbm = -94.0; // Typical thermal noise for 10 MHz BW at room temp
    double receivedPowerDbm = pathloss;
    double snr = receivedPowerDbm - noisePowerDbm;

    g_totalSnr += snr;
    g_totalPathloss += (receivedPowerDbm - 30.0); // Convert dBm to dB
    g_samples++;

    double t = Simulator::Now().GetSeconds();
    g_logFile << t << "," << txPos.x << "," << txPos.y << "," << rxPos.x << "," << rxPos.y << ","
              << snr << "," << (receivedPowerDbm - 30.0) << std::endl;

    Simulator::Schedule(Seconds(0.05), &LogSnrPathloss, txNode, rxNode, propLoss, channelModel, txAntennaPorts, rxAntennaPorts);
}

int main(int argc, char *argv[])
{
    // Simulation parameters
    uint32_t numVehicles = 10;
    std::string scenario = "Urban";
    double simTime = 20.0;
    double txPowerDbm = 23.0;
    uint32_t txAntennaRows = 4;
    uint32_t txAntennaColumns = 4;
    uint32_t rxAntennaRows = 2;
    uint32_t rxAntennaColumns = 2;
    double frequency = 2.0e9;
    double speed = 16.67; // ~60km/h

    CommandLine cmd;
    cmd.AddValue("numVehicles", "Number of vehicles", numVehicles);
    cmd.AddValue("scenario", "Mobility scenario: Urban or Highway", scenario);
    cmd.Parse(argc, argv);

    // Create vehicle nodes
    NodeContainer vehicles;
    vehicles.Create(numVehicles);

    // Configure Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

    if (scenario == "Urban")
    {
        // Place vehicles on a grid inside a city-like environment
        uint32_t gridSize = std::ceil(std::sqrt(numVehicles));
        double spacing = 30.0;
        for (uint32_t i = 0; i < numVehicles; ++i)
        {
            double x = (i % gridSize) * spacing + 5.0 * (i % 2); // Add random offset
            double y = (i / gridSize) * spacing + 3.0 * (i % 3);
            positionAlloc->Add(Vector(x, y, 1.5));
        }
        mobility.SetPositionAllocator(positionAlloc);
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                 "Bounds", RectangleValue(Rectangle(0, 500, 0, 500)),
                                 "Mode", StringValue("Time"),
                                 "Time", TimeValue(Seconds(2.0)),
                                 "Speed", StringValue("ns3::ConstantRandomVariable[Constant=6.0]"));
    }
    else // Highway
    {
        // Place vehicles on a highway, in a straight line
        double initialY = 50.0;
        for (uint32_t i = 0; i < numVehicles; ++i)
        {
            double x = i * 20.0;
            positionAlloc->Add(Vector(x, initialY, 1.5));
        }
        mobility.SetPositionAllocator(positionAlloc);
        mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    }
    mobility.Install(vehicles);

    if (scenario == "Highway")
    {
        // Set constant speed for highway mode
        for (uint32_t i = 0; i < numVehicles; ++i)
        {
            Ptr<ConstantVelocityMobilityModel> mob = vehicles.Get(i)->GetObject<ConstantVelocityMobilityModel>();
            mob->SetVelocity(Vector(speed, 0, 0));
        }
    }

    // Building scenario for 3GPP Models
    BuildingsHelper::Install(vehicles);

    // Channel and Propagation
    Ptr<ThreeGppPropagationLossModel> threeGppLoss;
    Ptr<ThreeGppChannelModel> threeGppChannel;

    if (frequency < 6.0e9)
    {
        threeGppLoss = CreateObject<ThreeGppTr385PropagationLossModel>();
        threeGppLoss->SetAttribute("Scenario", StringValue(scenario == "Urban" ? "UMi-StreetCanyon" : "RMa"));
    }
    else
    {
        threeGppLoss = CreateObject<ThreeGppTr38901PropagationLossModel>();
        threeGppLoss->SetAttribute("Scenario", StringValue(scenario == "Urban" ? "umi-street-canyon" : "rma"));
    }
    threeGppLoss->SetAttribute("Frequency", DoubleValue(frequency / 1e9));

    threeGppChannel = CreateObject<ThreeGppChannelModel>();
    threeGppChannel->SetAttribute("PropagationLoss", PointerValue(threeGppLoss));
    threeGppChannel->SetAttribute("UpdatePeriod", TimeValue(MilliSeconds(10)));

    // Antenna arrays: Uniform Planar Arrays & DFT beamforming
    DfAntennaArrayModelHelper dfHelper;
    dfHelper.Set("Rows", UintegerValue(txAntennaRows));
    dfHelper.Set("Columns", UintegerValue(txAntennaColumns));
    dfHelper.Set("ElementSpacing", DoubleValue(0.5));

    std::vector<Ptr<AntennaArrayModel>> txAntennaModels;
    for (uint32_t i = 0; i < numVehicles; ++i)
    {
        Ptr<DfAntennaArrayModel> txArray = dfHelper.Create();
        txAntennaModels.push_back(txArray);
        vehicles.Get(i)->AggregateObject(txArray);
    }
    // Receiver antennas
    DfAntennaArrayModelHelper rxHelper;
    rxHelper.Set("Rows", UintegerValue(rxAntennaRows));
    rxHelper.Set("Columns", UintegerValue(rxAntennaColumns));
    rxHelper.Set("ElementSpacing", DoubleValue(0.5));

    std::vector<Ptr<AntennaArrayModel>> rxAntennaModels;
    for (uint32_t i = 0; i < numVehicles; ++i)
    {
        Ptr<DfAntennaArrayModel> rxArray = rxHelper.Create();
        rxAntennaModels.push_back(rxArray);
        vehicles.Get(i)->AggregateObject(rxArray);
    }

    // Output log file
    g_logFile.open("vehicular-3gpp-snrandpathloss.csv");
    g_logFile << "Time,TxX,TxY,RxX,RxY,SNR_dB,Pathloss_dB\n";

    // For simplicity, monitor SNR/pathloss for the first vehicle pair (0->1)
    Simulator::ScheduleNow(&LogSnrPathloss, vehicles.Get(0), vehicles.Get(1),
                           threeGppLoss, threeGppChannel,
                           txAntennaRows * txAntennaColumns, rxAntennaRows * rxAntennaColumns);

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Final stats
    g_logFile.close();
    if (g_samples > 0)
    {
        std::ofstream avgFile("vehicular-averages.txt", std::ios::out);
        avgFile << "Average SNR (dB): " << (g_totalSnr / g_samples) << std::endl;
        avgFile << "Average Pathloss (dB): " << (g_totalPathloss / g_samples) << std::endl;
        avgFile.close();
    }

    Simulator::Destroy();
    return 0;
}