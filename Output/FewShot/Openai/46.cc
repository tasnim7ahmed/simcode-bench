#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/energy-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocWifiEnergyExample");

static void
RemainingEnergyTrace(double oldValue, double newValue)
{
    static std::ofstream energyLog("energy-trace.log", std::ios_base::app);
    energyLog << Simulator::Now().GetSeconds() << "s RemainingEnergy: " << newValue << std::endl;
}

static void
DeviceStateChangedTrace(WifiPhyState oldState, WifiPhyState newState)
{
    static std::ofstream stateLog("state-trace.log", std::ios_base::app);
    stateLog << Simulator::Now().GetSeconds() << "s WifiPhyState: " << oldState << " -> " << newState << std::endl;
}

static void
EnergyDepletionCallback()
{
    static std::ofstream stateLog("state-trace.log", std::ios_base::app);
    stateLog << Simulator::Now().GetSeconds() << "s Node entered SLEEP/DEPLETED state due to energy depletion!" << std::endl;
}

int main(int argc, char *argv[])
{
    double simTime = 30.0;
    std::string dataRate = "5Mbps";
    uint32_t packetSize = 512;
    double txPower = 16.0; // in dBm
    double initialEnergy = 5.0; // in Joules
    double supplyVoltage = 3.0;
    double idleCurrent = 0.273; // in A
    double txCurrent = 0.380; // in A
    double rxCurrent = 0.313; // in A
    double sleepCurrent = 0.045; // in A

    CommandLine cmd;
    cmd.AddValue("dataRate", "OnOffApplication Data Rate", dataRate);
    cmd.AddValue("packetSize", "OnOffApplication Packet Size (bytes)", packetSize);
    cmd.AddValue("txPower", "WiFi TX Power (dBm)", txPower);
    cmd.AddValue("initialEnergy", "Initial Energy (Joules)", initialEnergy);
    cmd.AddValue("supplyVoltage", "Supply Voltage (V)", supplyVoltage);
    cmd.AddValue("idleCurrent", "Idle current (A)", idleCurrent);
    cmd.AddValue("txCurrent", "Transmit current (A)", txCurrent);
    cmd.AddValue("rxCurrent", "Receive current (A)", rxCurrent);
    cmd.AddValue("sleepCurrent", "Sleep current (A)", sleepCurrent);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    // Set transmission power
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.Set("TxPowerStart", DoubleValue(txPower));
    wifiPhy.Set("TxPowerEnd", DoubleValue(txPower));
    wifiPhy.Set("EnergyDetectionThreshold", DoubleValue(-99.0));
    wifiPhy.Set("CcaMode1Threshold", DoubleValue(-99.0));

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(30.0, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Internet Stack
    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Energy Model
    BasicEnergySourceHelper energySourceHelper;
    energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(initialEnergy));
    energySourceHelper.Set("SupplyVoltageV", DoubleValue(supplyVoltage));
    EnergySourceContainer sources = energySourceHelper.Install(nodes);

    WifiRadioEnergyModelHelper radioEnergyHelper;
    radioEnergyHelper.Set("TxCurrentA", DoubleValue(txCurrent));
    radioEnergyHelper.Set("RxCurrentA", DoubleValue(rxCurrent));
    radioEnergyHelper.Set("IdleCurrentA", DoubleValue(idleCurrent));
    radioEnergyHelper.Set("SleepCurrentA", DoubleValue(sleepCurrent));
    DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install(devices, sources);

    // Attach trace sources to node 0 (sending node)
    Ptr<BasicEnergySource> src = DynamicCast<BasicEnergySource>(sources.Get(0));
    src->TraceConnectWithoutContext("RemainingEnergy", MakeCallback(&RemainingEnergyTrace));

    Ptr<NetDevice> dev = devices.Get(0);
    Ptr<WifiNetDevice> wifiDev = DynamicCast<WifiNetDevice>(dev);
    NS_ASSERT(wifiDev);
    Ptr<WifiPhy> phy = wifiDev->GetPhy();
    phy->TraceConnectWithoutContext("State", MakeCallback(&DeviceStateChangedTrace));

    Ptr<DeviceEnergyModel> energyModel = deviceModels.Get(0);
    energyModel->TraceConnectWithoutContext("TotalEnergyDepleted", MakeCallback(&EnergyDepletionCallback));

    // On-Off traffic: Node 0 -> Node 1
    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    onoff.SetAttribute("DataRate", StringValue(dataRate));
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer app = onoff.Install(nodes.Get(0));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(simTime));

    // Sink on node 1
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simTime));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}