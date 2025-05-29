#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/energy-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("EnergyEfficientWirelessSimulation");

void
LogReceiverEnergy (Ptr<BasicEnergySource> energySource, Ptr<EnergyHarvester> harvester)
{
    double residual = energySource->GetRemainingEnergy ();
    double harvested = 0.0;

    // Try to get the last harvested power
    Ptr<BasicEnergyHarvester> basicHarvester = DynamicCast<BasicEnergyHarvester> (harvester);
    if (basicHarvester)
    {
        harvested = basicHarvester->GetInstantaneousPower ();
    }

    Time now = Simulator::Now ();
    std::cout << now.GetSeconds () << "s: Receiver node: "
              << "Residual Energy = " << residual << " J, "
              << "Harvested Power = " << harvested << " W"
              << std::endl;

    if (Simulator::Now ().GetSeconds () < 10.0)
    {
        Simulator::Schedule (Seconds (1.0), &LogReceiverEnergy, energySource, harvester);
    }
}

void
PrintFinalStatistics (Ptr<BasicEnergySource> receiverSource, double initialEnergy)
{
    double residual = receiverSource->GetRemainingEnergy ();
    double consumed = initialEnergy - residual;

    std::cout << "\n\n==== FINAL STATISTICS ====" << std::endl;
    std::cout << "Initial energy:     " << initialEnergy << " J" << std::endl;
    std::cout << "Final residual energy: " << residual << " J" << std::endl;
    std::cout << "Total energy consumed: " << consumed << " J" << std::endl;
    std::cout << "=========================" << std::endl;
}

int
main (int argc, char *argv[])
{
    // Simulation parameters
    double simulationTime = 10.0; // seconds
    double initialEnergyJ = 1.0; // joules
    double txCurrentA = 0.0174; // amps
    double rxCurrentA = 0.0197; // amps
    double supplyVoltage = 3.0; // V for WiFi
    uint32_t packetSize = 1000; // bytes (arbitrary reasonable size)
    double packetInterval = 1.0; // seconds
    double txTime = 0.0023; // seconds for each packet

    CommandLine cmd;
    cmd.Parse (argc, argv);

    // Nodes
    NodeContainer nodes;
    nodes.Create (2);

    // WiFi setup
    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    wifiPhy.Set ("TxPowerStart", DoubleValue (16.0));
    wifiPhy.Set ("TxPowerEnd", DoubleValue (16.0));

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    wifiPhy.SetChannel (wifiChannel.Create ());

    WifiMacHelper wifiMac;
    wifiMac.SetType ("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

    // Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (nodes);

    // Internet stack
    InternetStackHelper internet;
    internet.Install (nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

    // Energy sources and devices
    BasicEnergySourceHelper basicSourceHelper;
    basicSourceHelper.Set ("BasicEnergySourceInitialEnergyJ", DoubleValue (initialEnergyJ));
    basicSourceHelper.Set ("SupplyVoltage", DoubleValue (supplyVoltage));
    EnergySourceContainer sources = basicSourceHelper.Install (nodes);

    // WiFi radio energy model
    WifiRadioEnergyModelHelper wifiRadioEnergyHelper;
    wifiRadioEnergyHelper.Set ("TxCurrentA", DoubleValue (txCurrentA));
    wifiRadioEnergyHelper.Set ("RxCurrentA", DoubleValue (rxCurrentA));
    // Optionally, can adjust IdleCurrentA, SleepCurrentA if desired

    DeviceEnergyModelContainer deviceModels = wifiRadioEnergyHelper.Install (devices, sources);

    // Energy Harvester configuration
    BasicEnergyHarvesterHelper harvesterHelper;
    harvesterHelper.Set ("PeriodicHarvestedPowerUpdateInterval", TimeValue (Seconds (1.0)));
    harvesterHelper.Set ("HarvestedPowerUpdateFunction", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=0.1]"));
    EnergyHarvesterContainer harvesters = harvesterHelper.Install (sources);

    // UDP application setup
    uint16_t port = 4000;
    // Receiver (node 1)
    UdpServerHelper server (port);
    ApplicationContainer apps = server.Install (nodes.Get (1));
    apps.Start (Seconds (0.0));
    apps.Stop (Seconds (simulationTime + 1.0));
    // Sender (node 0)
    UdpClientHelper client (interfaces.GetAddress (1), port);
    client.SetAttribute ("MaxPackets", UintegerValue (10));
    client.SetAttribute ("Interval", TimeValue (Seconds (packetInterval)));
    client.SetAttribute ("PacketSize", UintegerValue (packetSize));
    ApplicationContainer clientApp = client.Install (nodes.Get (0));
    clientApp.Start (Seconds (0.0));
    clientApp.Stop (Seconds (simulationTime + 1.0));

    // Logging energy every second for receiver node (node 1)
    Ptr<BasicEnergySource> receiverSource = DynamicCast<BasicEnergySource> (sources.Get (1));
    Ptr<EnergyHarvester> receiverHarvester = harvesters.Get (1);

    Simulator::Schedule (Seconds (0.0), &LogReceiverEnergy, receiverSource, receiverHarvester);

    // Run simulation
    Simulator::Stop (Seconds (simulationTime + 1.0));
    Simulator::Run ();

    // Print final stats for receiver node
    PrintFinalStatistics (receiverSource, initialEnergyJ);

    Simulator::Destroy ();
    return 0;
}