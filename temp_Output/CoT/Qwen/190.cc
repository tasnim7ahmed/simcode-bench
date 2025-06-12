#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/olsr-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/dsr-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetSimulation");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 3;
    double duration = 10.0;
    std::string phyMode("DsssRate1Mbps");
    bool enableNetAnim = true;

    CommandLine cmd;
    cmd.AddValue("numNodes", "Number of nodes in the MANET.", numNodes);
    cmd.AddValue("duration", "Duration of simulation in seconds.", duration);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    YansWifiPhyHelper wifiPhy;
    WifiChannelHelper wifiChannel = WifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    wifiPhy.Set("TxPowerStart", DoubleValue(5.0));
    wifiPhy.Set("TxPowerEnd", DoubleValue(5.0));
    wifiPhy.Set("TxGain", DoubleValue(0));
    wifiPhy.Set("RxGain", DoubleValue(0));
    wifiPhy.Set("EnergyDetectionThreshold", DoubleValue(-61.8));
    wifiPhy.Set("CcaMode1Threshold", DoubleValue(-64.8));

    WifiMacHelper wifiMac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"));
    mobility.SetMobilityModel("ns3::RandomDirection2DMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 500, 0, 500)),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.2]"));
    mobility.Install(nodes);

    InternetStackHelper stack;
    AodvHelper aodv;
    OlsrHelper olsr;
    DsdvHelper dsdv;
    DsrHelper dsr;
    stack.SetRoutingHelper(aodv);
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(numNodes - 1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(duration));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(numNodes - 1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(20));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numNodes - 1; ++i) {
        clientApps.Add(echoClient.Install(nodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(duration - 1.0));

    if (enableNetAnim) {
        AnimationInterface anim("manet_simulation.xml");
        anim.SetMobilityPollInterval(Seconds(0.1));
        anim.EnablePacketMetadata(true);
    }

    Simulator::Stop(Seconds(duration));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}