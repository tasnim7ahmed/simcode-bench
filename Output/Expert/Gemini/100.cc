#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/global-route-manager.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool tracing = true;
    std::string csmaSpeed = "100Mbps";

    CommandLine cmd;
    cmd.AddValue("tracing", "Enable or disable tracing", tracing);
    cmd.AddValue("csmaSpeed", "CSMA link speed (10Mbps or 100Mbps)", csmaSpeed);
    cmd.Parse(argc, argv);

    NodeContainer topRouters;
    topRouters.Create(1);
    NodeContainer bottomRouters;
    bottomRouters.Create(1);

    NodeContainer topSwitches;
    topSwitches.Create(4);
    NodeContainer bottomSwitches;
    bottomSwitches.Create(5);

    NodeContainer topEndpoints;
    topEndpoints.Create(2);
    NodeContainer bottomEndpoints;
    bottomEndpoints.Create(2);

    InternetStackHelper stack;
    stack.Install(topRouters);
    stack.Install(bottomRouters);
    stack.Install(topSwitches);
    stack.Install(bottomSwitches);
    stack.Install(topEndpoints);
    stack.Install(bottomEndpoints);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("50ms"));

    NetDeviceContainer routerDevicesTop = p2p.Install(topRouters.Get(0), bottomRouters.Get(0));

    CsmaHelper csmaTop;
    csmaTop.SetChannelAttribute("DataRate", StringValue(csmaSpeed));
    csmaTop.SetChannelAttribute("Delay", StringValue("2us"));

    NetDeviceContainer topSwitchDevices1 = csmaTop.Install(NodeContainer(topRouters.Get(0), topSwitches.Get(0)));
    NetDeviceContainer topSwitchDevices2 = csmaTop.Install(NodeContainer(topSwitches.Get(0), topSwitches.Get(1)));
    NetDeviceContainer topSwitchDevices3 = csmaTop.Install(NodeContainer(topSwitches.Get(1), topSwitches.Get(2)));
    NetDeviceContainer topSwitchDevices4 = csmaTop.Install(NodeContainer(topSwitches.Get(2), topSwitches.Get(3)));
    NetDeviceContainer topEndpointDevices1 = csmaTop.Install(NodeContainer(topSwitches.Get(3), topEndpoints.Get(0)));
    NetDeviceContainer topEndpointDevices2 = csmaTop.Install(NodeContainer(topSwitches.Get(1), topEndpoints.Get(1)));

    CsmaHelper csmaBottom;
    csmaBottom.SetChannelAttribute("DataRate", StringValue(csmaSpeed));
    csmaBottom.SetChannelAttribute("Delay", StringValue("2us"));

    NetDeviceContainer bottomSwitchDevices1 = csmaBottom.Install(NodeContainer(bottomRouters.Get(0), bottomSwitches.Get(0)));
    NetDeviceContainer bottomSwitchDevices2 = csmaBottom.Install(NodeContainer(bottomSwitches.Get(0), bottomSwitches.Get(1)));
    NetDeviceContainer bottomSwitchDevices3 = csmaBottom.Install(NodeContainer(bottomSwitches.Get(1), bottomSwitches.Get(2)));
    NetDeviceContainer bottomSwitchDevices4 = csmaBottom.Install(NodeContainer(bottomSwitches.Get(2), bottomSwitches.Get(3)));
    NetDeviceContainer bottomSwitchDevices5 = csmaBottom.Install(NodeContainer(bottomSwitches.Get(3), bottomSwitches.Get(4)));
    NetDeviceContainer bottomEndpointDevices1 = csmaBottom.Install(NodeContainer(bottomSwitches.Get(4), bottomEndpoints.Get(0)));
    NetDeviceContainer bottomEndpointDevices2 = csmaBottom.Install(NodeContainer(bottomSwitches.Get(2), bottomEndpoints.Get(1)));

    Ipv4AddressHelper address;
    address.SetBase("76.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer routerInterfaces = address.Assign(routerDevicesTop);

    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer topSwitchInterfaces1 = address.Assign(topSwitchDevices1);
    Ipv4InterfaceContainer topSwitchInterfaces2 = address.Assign(topSwitchDevices2);
    Ipv4InterfaceContainer topSwitchInterfaces3 = address.Assign(topSwitchDevices3);
    Ipv4InterfaceContainer topSwitchInterfaces4 = address.Assign(topSwitchDevices4);
    Ipv4InterfaceContainer topEndpointInterfaces1 = address.Assign(topEndpointDevices1);
    Ipv4InterfaceContainer topEndpointInterfaces2 = address.Assign(topEndpointDevices2);

    address.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer bottomSwitchInterfaces1 = address.Assign(bottomSwitchDevices1);
    Ipv4InterfaceContainer bottomSwitchInterfaces2 = address.Assign(bottomSwitchDevices2);
    Ipv4InterfaceContainer bottomSwitchInterfaces3 = address.Assign(bottomSwitchDevices3);
    Ipv4InterfaceContainer bottomSwitchInterfaces4 = address.Assign(bottomSwitchDevices4);
    Ipv4InterfaceContainer bottomSwitchInterfaces5 = address.Assign(bottomSwitchDevices5);
    Ipv4InterfaceContainer bottomEndpointInterfaces1 = address.Assign(bottomEndpointDevices1);
    Ipv4InterfaceContainer bottomEndpointInterfaces2 = address.Assign(bottomEndpointDevices2);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    UdpEchoServerHelper echoServer1(9);
    ApplicationContainer serverApps1 = echoServer1.Install(bottomEndpoints.Get(0));
    serverApps1.Start(Seconds(1.0));
    serverApps1.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient1(bottomEndpointInterfaces1.GetAddress(1), 9);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps1 = echoClient1.Install(topEndpoints.Get(0));
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(10.0));

    UdpEchoServerHelper echoServer2(9);
    ApplicationContainer serverApps2 = echoServer2.Install(bottomEndpoints.Get(1));
    serverApps2.Start(Seconds(1.0));
    serverApps2.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient2(bottomEndpointInterfaces2.GetAddress(1), 9);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps2 = echoClient2.Install(topEndpoints.Get(1));
    clientApps2.Start(Seconds(2.0));
    clientApps2.Stop(Seconds(10.0));

    if (tracing) {
        p2p.EnablePcapAll("wan");
        csmaTop.EnablePcapAll("top");
        csmaBottom.EnablePcapAll("bottom");
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}