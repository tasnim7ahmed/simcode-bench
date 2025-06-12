#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/object-name-service-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ObjectNameExample");

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Assign names to nodes
    Names::Add("client", nodes.Get(0));
    Names::Add("server", nodes.Get(1));
    Names::Add("node2", nodes.Get(2));
    Names::Add("node3", nodes.Get(3));

    // Rename client node
    Names::Rename("client", "workstation");

    // Create CSMA channel
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
    csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(2)));

    // Install devices
    NetDeviceContainer devices = csma.Install(nodes);

    // Assign names to devices
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        std::ostringstream oss;
        oss << "device" << i;
        Names::Add(oss.str(), devices.Get(i));
    }

    // Setup internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Configure server and client using object names
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(Names::Find<Node>("server"));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(Names::Find<Node>("workstation"));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable pcap tracing on all devices
    csma.EnablePcapAll("object-names-example");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}