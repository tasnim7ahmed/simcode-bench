#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/dsr-module.h"
#include "ns3/sumo-mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(10);

    // Configure SUMO mobility
    SumoMobilityHelper sumo;
    sumo.sumoBinary = "sumo"; 
    sumo.configFile = "data/cross.sumocfg";
    sumo.traceFile = "data/cross.rou.xml";
    sumo.netFile = "data/cross.net.xml";
    sumo.prefix = "vanet";
    sumo.guisim = false;
    sumo.remotePort = 9999;
    sumo.Initialize();
    sumo.AssignStreams(nodes);

    // Configure Wi-Fi
    WifiHelper wifi;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
    Ssid ssid = Ssid("vanet-network");
    mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));

    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("OfdmRate6Mbps"),
                                "RtsCtsThreshold", UintegerValue(0));

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.SetRoutingHelper(DsrHelper());
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Configure UDP applications (example: node 0 sends to node 9)
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(9));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(80.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(9), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(80.0));

    // Run the simulation
    Simulator::Stop(Seconds(80.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}