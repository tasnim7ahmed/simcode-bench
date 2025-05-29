#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WSN");

int main(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Set up logging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(6);

    // Create CSMA channel
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(Seconds(0.002)));

    NetDeviceContainer csmaDevices = csma.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(csmaDevices);

    // Create UDP server (base station)
    UdpServerHelper server(9);
    ApplicationContainer serverApp = server.Install(nodes.Get(0)); // Node 0 is the base station
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Create UDP clients (sensor nodes)
    UdpClientHelper client(interfaces.GetAddress(0), 9); // Send to the base station
    client.SetAttribute("MaxPackets", UintegerValue(100));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < nodes.GetN(); ++i) {
        clientApps.Add(client.Install(nodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable PCAP tracing
    csma.EnablePcapAll("wsn");

    // Enable NetAnim
    AnimationInterface anim("wsn.xml");
    anim.SetConstantPosition(nodes.Get(0), 10.0, 10.0);
    anim.SetConstantPosition(nodes.Get(1), 20.0, 20.0);
    anim.SetConstantPosition(nodes.Get(2), 30.0, 10.0);
    anim.SetConstantPosition(nodes.Get(3), 40.0, 20.0);
    anim.SetConstantPosition(nodes.Get(4), 20.0, 30.0);
    anim.SetConstantPosition(nodes.Get(5), 30.0, 30.0);


    // Start simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}