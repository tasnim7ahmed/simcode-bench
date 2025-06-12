#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // Enable logging if needed
    // LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Set the simulation time and application settings
    double simTime = 10.0;           // seconds
    uint16_t port = 8080;            // TCP server port
    std::string dataRate = "5Mbps";  // Client send data rate
    std::string maxBytes = "0";      // 0 = unlimited send

    // Step 1: Create nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Step 2: Create CSMA channel
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    // Step 3: Install CSMA devices
    NetDeviceContainer devices = csma.Install(nodes);

    // Step 4: Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Step 5: Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Step 6: Create and setup the TCP server (PacketSink) on node 0
    Address localAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", localAddress);
    ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simTime));

    // Step 7: Create TCP clients (BulkSendApplications) on nodes 1,2,3
    for (uint32_t i = 1; i < nodes.GetN(); ++i)
    {
        // Destination is node 0
        BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory",
                                      InetSocketAddress(interfaces.GetAddress(0), port));
        bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited
        bulkSendHelper.SetAttribute("SendSize", UintegerValue(1024));
        ApplicationContainer clientApp = bulkSendHelper.Install(nodes.Get(i));
        clientApp.Start(Seconds(1.0));  // Start after server
        clientApp.Stop(Seconds(simTime));
    }

    // Enable PCAP tracing on all CSMA devices
    csma.EnablePcapAll("csma-tcp");

    // Optional: Enable ASCII trace
    // AsciiTraceHelper ascii;
    // csma.EnableAsciiAll(ascii.CreateFileStream("csma-tcp.tr"));

    // Run the simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}