#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

int main(int argc, char *argv[])
{
    ns3::NodeContainer nodes;
    nodes.Create(3);

    ns3::CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", ns3::StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", ns3::TimeValue(ns3::NanoSeconds(6560)));

    ns3::NetDeviceContainer csmaDevices = csma.Install(nodes);

    ns3::InternetStackHelper internet;
    internet.Install(nodes);

    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer ipv4Interfaces = ipv4.Assign(csmaDevices);

    ns3::Ipv4Address serverAddress = ipv4Interfaces.GetAddress(2);

    ns3::UdpEchoServerHelper echoServer(9);
    ns3::ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(ns3::Seconds(1.0));
    serverApps.Stop(ns3::Seconds(10.0));

    ns3::UdpEchoClientHelper echoClient(serverAddress, 9);
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(10));
    echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(1024));

    ns3::ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(ns3::Seconds(2.0));
    clientApps.Stop(ns3::Seconds(10.0));

    csma.EnablePcapAll("bus_topology_pcap");

    ns3::Simulator::Stop(ns3::Seconds(10.0));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}