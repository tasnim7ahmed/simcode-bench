#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/energy-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SensorNetworkOlsrSimulation");

int main(int argc, char *argv[])
{
    uint32_t nSensorNodes = 10;
    double simulationTime = 50.0;
    double appStartTime = 1.0;
    double appStopTime = 49.0;
    double txInterval = 2.0;
    uint16_t port = 5000;

    CommandLine cmd;
    cmd.AddValue("nSensorNodes", "Number of sensor nodes", nSensorNodes);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(nSensorNodes + 1); // last node is the sink

    // Wifi & Channel setup
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("DsssRate1Mbps"), "ControlMode", StringValue("DsssRate1Mbps"));

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue (0.0),
                                  "MinY", DoubleValue (0.0),
                                  "DeltaX", DoubleValue (20.0),
                                  "DeltaY", DoubleValue (20.0),
                                  "GridWidth", UintegerValue (4),
                                  "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Energy: Use BasicEnergySource and WifiRadioEnergyModel for each sensor node
    BasicEnergySourceHelper energySourceHelper;
    energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(2.0));
    EnergySourceContainer sources = energySourceHelper.Install(nodes.GetN() - 1 ? NodeContainer(nodes.Get(0), nodes.Get(nodes.GetN()-2)) : nodes);

    WifiRadioEnergyModelHelper radioEnergyHelper;
    radioEnergyHelper.Set("TxCurrentA", DoubleValue(0.0174));
    radioEnergyHelper.Set("RxCurrentA", DoubleValue(0.0197));
    radioEnergyHelper.Install(devices.GetN() - 1 ? NetDeviceContainer(devices.Begin(), devices.End() - 1) : devices, sources);

    // Internet stack + OLSR
    OlsrHelper olsr;
    InternetStackHelper stack;
    stack.SetRoutingHelper(olsr);
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Sink: last node
    Ptr<Node> sinkNode = nodes.Get(nSensorNodes);
    Ipv4Address sinkAddr = interfaces.GetAddress(nSensorNodes);

    // UDP sink application
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sinkHelper.Install(sinkNode);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simulationTime));

    // UDP sender applications on each sensor node
    for (uint32_t i = 0; i < nSensorNodes; ++i)
    {
        UdpClientHelper client(sinkAddr, port);
        client.SetAttribute("MaxPackets", UintegerValue((uint32_t)((appStopTime-appStartTime)/txInterval)));
        client.SetAttribute("Interval", TimeValue(Seconds(txInterval)));
        client.SetAttribute("PacketSize", UintegerValue(32));

        ApplicationContainer clientApp = client.Install(nodes.Get(i));
        clientApp.Start(Seconds(appStartTime));
        clientApp.Stop(Seconds(appStopTime));
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}