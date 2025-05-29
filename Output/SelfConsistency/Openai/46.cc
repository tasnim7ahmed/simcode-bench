#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/energy-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiAdhocEnergyDepletionExample");

// Callback for state transition
void
DeviceEnergyStateChangedCallback(
    int64_t oldState,
    int64_t newState)
{
    std::ofstream log("energy-state-transitions.log", std::ios::app);
    log << Simulator::Now().GetSeconds() << "s: Node entered state "
        << newState << " from " << oldState << std::endl;
    log.close();
}

void
RemainingEnergyCallback(double oldValue, double remainingEnergy)
{
    std::ofstream log("remaining-energy.log", std::ios::app);
    log << Simulator::Now().GetSeconds()
        << "s: Remaining energy: " << remainingEnergy << " Joules" << std::endl;
    log.close();
}

void
DepletionHandler()
{
    std::cout << Simulator::Now().GetSeconds()
              << "s: Node has entered sleep due to energy depletion!" << std::endl;
}

int
main(int argc, char *argv[])
{
    // Default values
    double simulationTime = 20.0; // seconds
    std::string dataRate = "1Mbps";
    uint32_t packetSize = 512;
    double txPower = 16.0; // dBm
    double phyTxCurrentA = 0.0174; // A
    double phyRxCurrentA = 0.0197; // A
    double idleCurrentA = 0.017; // A
    double sleepCurrentA = 0.00005; // A
    double initialEnergyJ = 0.1; // Joules

    // Command line options
    CommandLine cmd(__FILE__);
    cmd.AddValue("dataRate", "Application data rate", dataRate);
    cmd.AddValue("packetSize", "Application packet size (bytes)", packetSize);
    cmd.AddValue("txPower", "Transmission power in dBm", txPower);
    cmd.AddValue("phyTxCurrentA", "WiFi PHY Tx current (A)", phyTxCurrentA);
    cmd.AddValue("phyRxCurrentA", "WiFi PHY Rx current (A)", phyRxCurrentA);
    cmd.AddValue("idleCurrentA", "WiFi PHY idle current (A)", idleCurrentA);
    cmd.AddValue("sleepCurrentA", "WiFi PHY sleep current (A)", sleepCurrentA);
    cmd.AddValue("initialEnergyJ", "Initial energy (Joules)", initialEnergyJ);
    cmd.AddValue("simulationTime", "Simulation time (s)", simulationTime);
    cmd.Parse(argc, argv);

    // Create 2 nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Set up WiFi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    wifiPhy.Set("TxPowerStart", DoubleValue(txPower));
    wifiPhy.Set("TxPowerEnd", DoubleValue(txPower));
    wifiPhy.Set("EnergyDetectionThreshold", DoubleValue(-90.0));
    wifiPhy.Set("CcaMode1Threshold", DoubleValue(-90.0));

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");
    
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Set node positions (place nodes close enough for transmission)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(5.0),
                                 "DeltaY", DoubleValue(0.0),
                                 "GridWidth", UintegerValue(2),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install Internet stack & assign IP addresses
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // --- Energy Model ---
    BasicEnergySourceHelper energySourceHelper;
    energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(initialEnergyJ));

    EnergySourceContainer sources = energySourceHelper.Install(nodes);

    WifiRadioEnergyModelHelper wifiEnergyHelper;
    wifiEnergyHelper.Set("TxCurrentA", DoubleValue(phyTxCurrentA));
    wifiEnergyHelper.Set("RxCurrentA", DoubleValue(phyRxCurrentA));
    wifiEnergyHelper.Set("IdleCurrentA", DoubleValue(idleCurrentA));
    wifiEnergyHelper.Set("SleepCurrentA", DoubleValue(sleepCurrentA));

    DeviceEnergyModelContainer deviceModels = wifiEnergyHelper.Install(devices, sources);

    // Trace energy state and remaining energy for node 0
    Ptr<BasicEnergySource> energySource0 = DynamicCast<BasicEnergySource>(sources.Get(0));
    energySource0->TraceConnectWithoutContext("RemainingEnergy", MakeCallback(&RemainingEnergyCallback));

    Ptr<WifiRadioEnergyModel> wifiEnergyModel0 = DynamicCast<WifiRadioEnergyModel>(deviceModels.Get(0));
    wifiEnergyModel0->TraceConnectWithoutContext("StateChanged", MakeCallback(&DeviceEnergyStateChangedCallback));
    wifiEnergyModel0->SetDepletionCallback(MakeCallback(&DepletionHandler));

    // --- OnOff Application ---
    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    onoff.SetAttribute("DataRate", StringValue(dataRate));
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer apps = onoff.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(simulationTime - 1.0));
    
    // Sink at node 1
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simulationTime));

    // Enable PCAP traces (optional)
    wifiPhy.EnablePcap("wifi-adhoc-energy", devices.Get(0));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}