#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/bridge-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MultiSwitchBridgeTest");

int main(int argc, char *argv[]) {
    bool verbose = false;
    std::string lanLinkSpeed = "100Mbps";
    std::string wanLinkRate = "10Mbps";
    std::string wanLinkDelay = "2ms";

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if all packets received", verbose);
    cmd.AddValue("lanLinkSpeed", "LAN link data rate", lanLinkSpeed);
    cmd.AddValue("wanLinkRate", "WAN link data rate", wanLinkRate);
    cmd.AddValue("wanLinkDelay", "WAN link delay", wanLinkDelay);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    NodeContainer topNodes, bottomNodes, routers;
    topNodes.Create(3); // t1, t2, t3
    bottomNodes.Create(3); // b1, b2, b3
    routers.Create(2); // tr, br

    NodeContainer topSwitches, bottomSwitches;
    topSwitches.Create(2);
    bottomSwitches.Create(2);

    InternetStackHelper stack;
    stack.Install(topNodes);
    stack.Install(bottomNodes);
    stack.Install(routers);

    CsmaHelper csmaHelperLan;
    csmaHelperLan.SetChannelAttribute("DataRate", StringValue(lanLinkSpeed));
    csmaHelperLan.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    // Top LAN Topology
    NetDeviceContainer topRouterDevice = csmaHelperLan.Install(NodeContainer(topNodes.Get(0), routers.Get(0))); // t1-tr
    NetDeviceContainer topSwitch1Device = csmaHelperLan.Install(NodeContainer(topNodes.Get(1), topSwitches.Get(0))); // t2-s1
    NetDeviceContainer topSwitch2Device = csmaHelperLan.Install(NodeContainer(topSwitches.Get(0), topSwitches.Get(1))); // s1-s2
    NetDeviceContainer topSwitch3Device = csmaHelperLan.Install(NodeContainer(topSwitches.Get(1), routers.Get(0))); // s2-tr
    NetDeviceContainer topSwitch4Device = csmaHelperLan.Install(NodeContainer(topNodes.Get(2), topSwitches.Get(1))); // t3-s2


    // Bottom LAN Topology
    NetDeviceContainer bottomRouterDevice = csmaHelperLan.Install(NodeContainer(bottomNodes.Get(0), routers.Get(1))); // b1-br
    NetDeviceContainer bottomSwitch1Device = csmaHelperLan.Install(NodeContainer(bottomNodes.Get(1), bottomSwitches.Get(0))); // b2-s3
    NetDeviceContainer bottomSwitch2Device = csmaHelperLan.Install(NodeContainer(bottomSwitches.Get(0), bottomSwitches.Get(1))); // s3-s4
    NetDeviceContainer bottomSwitch3Device = csmaHelperLan.Install(NodeContainer(bottomSwitches.Get(1), routers.Get(1))); // s4-br
    NetDeviceContainer bottomSwitch4Device = csmaHelperLan.Install(NodeContainer(bottomNodes.Get(2), bottomSwitches.Get(1))); // b3-s4


    // WAN Topology
    PointToPointHelper pointToPointHelper;
    pointToPointHelper.SetDeviceAttribute("DataRate", StringValue(wanLinkRate));
    pointToPointHelper.SetChannelAttribute("Delay", StringValue(wanLinkDelay));
    NetDeviceContainer wanDevices = pointToPointHelper.Install(routers);


    // Bridge setup
    BridgeHelper bridgeHelper;
    bridgeHelper.Install(topSwitches.Get(0), NetDeviceContainer({topSwitch1Device.Get(1), topSwitch2Device.Get(0)}));
    bridgeHelper.Install(topSwitches.Get(1), NetDeviceContainer({topSwitch2Device.Get(1), topSwitch3Device.Get(0), topSwitch4Device.Get(1)}));

    bridgeHelper.Install(bottomSwitches.Get(0), NetDeviceContainer({bottomSwitch1Device.Get(1), bottomSwitch2Device.Get(0)}));
    bridgeHelper.Install(bottomSwitches.Get(1), NetDeviceContainer({bottomSwitch2Device.Get(1), bottomSwitch3Device.Get(0), bottomSwitch4Device.Get(1)}));



    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer topInterfaces = address.Assign(NetDeviceContainer({topRouterDevice.Get(0), topSwitch1Device.Get(0), topSwitch4Device.Get(0)}));
    Ipv4InterfaceContainer topSwitchInterfaces;
    address.Assign(NetDeviceContainer({topSwitch2Device.Get(0),topSwitch3Device.Get(0)}));
    Ipv4InterfaceContainer topRouterInterface;
    topRouterInterface.Add(topInterfaces.Get(0));
    topRouterInterface.Add(address.Assign(NetDeviceContainer({topSwitch3Device.Get(1)})).Get(0));
    address.Assign(NetDeviceContainer({topSwitch1Device.Get(1)}));


    address.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer bottomInterfaces = address.Assign(NetDeviceContainer({bottomRouterDevice.Get(0), bottomSwitch1Device.Get(0), bottomSwitch4Device.Get(0)}));
    Ipv4InterfaceContainer bottomSwitchInterfaces;
    address.Assign(NetDeviceContainer({bottomSwitch2Device.Get(0),bottomSwitch3Device.Get(0)}));
    Ipv4InterfaceContainer bottomRouterInterface;
    bottomRouterInterface.Add(bottomInterfaces.Get(0));
    bottomRouterInterface.Add(address.Assign(NetDeviceContainer({bottomSwitch3Device.Get(1)})).Get(0));
    address.Assign(NetDeviceContainer({bottomSwitch1Device.Get(1)}));

    Ipv4AddressHelper wanAddress;
    wanAddress.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wanInterfaces = wanAddress.Assign(wanDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverAppsTop = echoServer.Install(topNodes.Get(2));
    ApplicationContainer serverAppsBottom = echoServer.Install(bottomNodes.Get(1));
    serverAppsTop.Start(Seconds(1.0));
    serverAppsTop.Stop(Seconds(10.0));
    serverAppsBottom.Start(Seconds(1.0));
    serverAppsBottom.Stop(Seconds(10.0));


    UdpEchoClientHelper echoClientTop(topInterfaces.Get(2), 9);
    echoClientTop.SetAttribute("MaxPackets", UintegerValue(1));
    echoClientTop.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClientTop.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientAppsTop = echoClientTop.Install(topNodes.Get(1));
    clientAppsTop.Start(Seconds(2.0));
    clientAppsTop.Stop(Seconds(10.0));


    UdpEchoClientHelper echoClientBottom(bottomInterfaces.Get(2), 9);
    echoClientBottom.SetAttribute("MaxPackets", UintegerValue(1));
    echoClientBottom.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClientBottom.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientAppsBottom = echoClientBottom.Install(bottomNodes.Get(2));
    clientAppsBottom.Start(Seconds(2.0));
    clientAppsBottom.Stop(Seconds(10.0));



    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}