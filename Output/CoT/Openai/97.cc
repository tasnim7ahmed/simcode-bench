#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/object-names.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Assign human-readable names to nodes
    Ptr<Node> node0 = nodes.Get(0);
    Ptr<Node> node1 = nodes.Get(1);
    Ptr<Node> node2 = nodes.Get(2);
    Ptr<Node> node3 = nodes.Get(3);

    Names::Add("client", node0);
    Names::Add("intermediate1", node1);
    Names::Add("intermediate2", node2);
    Names::Add("server", node3);

    // Create CSMA LAN
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1500));

    NetDeviceContainer devices = csma.Install(nodes);

    // Assign names to NetDevices
    for (uint32_t i = 0; i < devices.GetN(); ++i)
    {
        std::ostringstream devName;
        devName << Names::FindName(nodes.Get(i)) << "-if";
        Names::Add(devName.str(), devices.Get(i));
    }

    // Demonstrate renaming
    Names::Rename("intermediate1", "midpoint1"); // Renaming node1
    Names::Rename("intermediate2", "midpoint2"); // Renaming node2

    // Configure attributes using the object name service
    // MTU of "client-if"
    Config::Set("/Names/client-if/Mtu", UintegerValue(1400));
    // Mix object names and node attributes: set node attribute via Named path
    Config::Set("/Names/server/$ns3::Node/DeviceAddition", BooleanValue(true));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install UDP Echo Server on "server"
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(Names::Find<Node>("server"));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Install UDP Echo Client on "client"
    UdpEchoClientHelper echoClient(interfaces.GetAddress(3), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(Names::Find<Node>("client"));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable packet tracing and pcap
    csma.EnablePcapAll("csma-object-names", true);

    // Enable ASCII tracing as well (optional)
    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("csma-object-names.tr"));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}