#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

// Do not use namespace ns3; in header files, but it's fine in .cc files
// or within main. For this specific task, direct use of ns3:: is preferred.

int main (int argc, char *argv[])
{
    // 1. Command-line options
    bool enableTracing = false;
    bool nanosecTrace = false;

    ns3::CommandLine cmd (__FILE__);
    cmd.AddValue ("enableTracing", "Enable pcap tracing.", enableTracing);
    cmd.AddValue ("nanosecTrace", "Use nanosecond timestamps for pcap trace files.", nanosecTrace);
    cmd.Parse (argc, argv);

    // Set default time resolution to microseconds
    // This needs to be set BEFORE any ns3::Time objects are created and potentially before tracing
    // is configured if nanosecTrace is true, as it influences how timestamps are handled.
    ns3::Time::SetResolution (ns3::Time::US);

    // 2. Create nodes
    ns3::NodeContainer nodes;
    nodes.Create (2); // n0 and n1

    // 3. Setup Point-to-Point link
    ns3::PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", ns3::StringValue ("500Kbps"));
    p2p.SetChannelAttribute ("Delay", ns3::StringValue ("5ms"));

    ns3::NetDeviceContainer devices = p2p.Install (nodes);

    // 4. Install Internet Stack
    ns3::InternetStackHelper stack;
    stack.Install (nodes);

    // 5. Assign IP Addresses
    ns3::Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = address.Assign (devices);

    // 6. Setup Applications

    // PacketSink on n1 (receiver)
    uint16_t sinkPort = 9; // Arbitrary port number
    ns3::Address sinkAddress (ns3::InetSocketAddress (interfaces.GetAddress (1), sinkPort)); // n1's IP address and port
    ns3::PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkAddress);
    ns3::ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (1)); // Install on n1
    sinkApp.Start (ns3::Seconds (0.0));
    sinkApp.Stop (ns3::Seconds (10.0));

    // BulkSendApplication on n0 (sender)
    ns3::BulkSendHelper bulkSendHelper ("ns3::TcpSocketFactory", ns3::Address (ns3::InetSocketAddress (interfaces.GetAddress (1), sinkPort)));
    // Set MaxBytes to 0 for unlimited sending
    bulkSendHelper.SetAttribute ("MaxBytes", ns3::UintegerValue (0));
    ns3::ApplicationContainer sourceApp = bulkSendHelper.Install (nodes.Get (0)); // Install on n0
    sourceApp.Start (ns3::Seconds (1.0)); // Start after 1 second
    sourceApp.Stop (ns3::Seconds (9.0));  // Stop 1 second before simulation ends

    // 7. Enable Tracing (if requested)
    if (enableTracing)
    {
        // Use Pcap tracing for devices
        // The file name should be "tcp-pcap-nanosec-example.pcap"
        // The nanosecTrace flag determines timestamp resolution
        p2p.EnablePcap ("tcp-pcap-nanosec-example", devices, nanosecTrace);
    }

    // 8. Run Simulation
    ns3::Simulator::Stop (ns3::Seconds (10.0)); // Simulation runs for 10 seconds
    ns3::Simulator::Run ();
    ns3::Simulator::Destroy ();

    return 0;
}