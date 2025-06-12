#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(4);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // TCP server on node 0
    uint16_t port = 8080;
    Address serverAddress(InetSocketAddress(interfaces.GetAddress(0), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    // TCP clients on nodes 1, 2, 3
    for (uint32_t i = 1; i < 4; ++i)
    {
        OnOffHelper clientHelper("ns3::TcpSocketFactory", serverAddress);
        clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
        clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
        clientHelper.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
        clientHelper.SetAttribute("StopTime", TimeValue(Seconds(9.0)));
        clientHelper.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited

        ApplicationContainer clientApp = clientHelper.Install(nodes.Get(i));
    }

    csma.EnablePcapAll("csma-tcp", false);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}