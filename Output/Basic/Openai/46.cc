#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/energy-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocWifiEnergyDepletion");

std::ofstream energyTrace;
std::ofstream stateTrace;

void
RemainingEnergyCallback(double oldValue, double newValue)
{
    energyTrace << Simulator::Now().GetSeconds() << " " << newValue << std::endl;
}

void
DeviceStateChangedCallback(uint32_t oldState, uint32_t newState)
{
    stateTrace << Simulator::Now().GetSeconds() << " " << oldState << " " << newState << std::endl;
    if (newState == WifiPhyState::WifiPhyStateSleep)
    {
        NS_LOG_UNCOND("Device entered sleep state at " << Simulator::Now().GetSeconds());
    }
}

void
EnergyDepletionCallback()
{
    NS_LOG_UNCOND("Node energy fully depleted at " << Simulator::Now().GetSeconds());
}

int
main(int argc, char *argv[])
{
    double simTime = 30.0;
    std::string dataRate = "1Mbps";
    uint32_t packetSize = 512;
    double txPowerStart = 16.0;
    double txPowerEnd = 16.0;
    double phyTxCurrentA = 0.0174;
    double phyRxCurrentA = 0.0197;
    double idleCurrentA = 0.273e-3;
    double sleepCurrentA = 0.009e-3;
    double initialEnergy = 0.2; // Joules

    CommandLine cmd(__FILE__);
    cmd.AddValue("dataRate", "OnOffApplication data rate", dataRate);
    cmd.AddValue("packetSize", "OnOffApplication packet size (bytes)", packetSize);
    cmd.AddValue("txPowerStart", "Wifi Tx power start (dBm)", txPowerStart);
    cmd.AddValue("txPowerEnd", "Wifi Tx power end (dBm)", txPowerEnd);
    cmd.AddValue("phyTxCurrentA", "Tx current (A)", phyTxCurrentA);
    cmd.AddValue("phyRxCurrentA", "Rx current (A)", phyRxCurrentA);
    cmd.AddValue("idleCurrentA", "Idle current (A)", idleCurrentA);
    cmd.AddValue("sleepCurrentA", "Sleep current (A)", sleepCurrentA);
    cmd.AddValue("initialEnergy", "Initial node energy (J)", initialEnergy);
    cmd.Parse(argc, argv);

    energyTrace.open("energy-trace.txt");
    stateTrace.open("state-trace.txt");

    NodeContainer nodes;
    nodes.Create(2);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.Set("TxPowerStart", DoubleValue(txPowerStart));
    wifiPhy.Set("TxPowerEnd", DoubleValue(txPowerEnd));
    wifiPhy.Set("RxNoiseFigure", DoubleValue(7.0));
    wifiPhy.Set("EnergyDetectionThreshold", DoubleValue(-89.0));
    wifiPhy.Set("CcaMode1Threshold", DoubleValue(-62.0));
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(10.0, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    BasicEnergySourceHelper energySourceHelper;
    energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(initialEnergy));
    EnergySourceContainer sources = energySourceHelper.Install(nodes);

    WifiRadioEnergyModelHelper radioEnergyHelper;
    radioEnergyHelper.Set("TxCurrentA", DoubleValue(phyTxCurrentA));
    radioEnergyHelper.Set("RxCurrentA", DoubleValue(phyRxCurrentA));
    radioEnergyHelper.Set("IdleCurrentA", DoubleValue(idleCurrentA));
    radioEnergyHelper.Set("SleepCurrentA", DoubleValue(sleepCurrentA));
    DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install(devices, sources);

    Ptr<BasicEnergySource> source = DynamicCast<BasicEnergySource>(sources.Get(0));
    source->TraceConnectWithoutContext("RemainingEnergy", MakeCallback(&RemainingEnergyCallback));
    Ptr<DeviceEnergyModel> devModel = deviceModels.Get(0);
    devModel->TraceConnectWithoutContext("StateChanged", MakeCallback(&DeviceStateChangedCallback));
    devModel->SetDepletionCallback(MakeCallback(&EnergyDepletionCallback));

    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    onoff.SetConstantRate(DataRate(dataRate), packetSize);
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(simTime)));

    ApplicationContainer app = onoff.Install(nodes.Get(0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simTime));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    energyTrace.close();
    stateTrace.close();

    return 0;
}