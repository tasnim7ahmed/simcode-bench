#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/queue.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpStarServer");

int main(int argc, char* argv[]) {
    // Set up command line arguments
    int nNodes = 9;
    uint32_t packetSize = 1472;
    std::string dataRate = "10Mbps";

    CommandLine cmd;
    cmd.AddValue("nNodes", "Number of nodes", nNodes);
    cmd.AddValue("packetSize", "Size of packets", packetSize);
    cmd.AddValue("dataRate", "Data rate of CBR traffic", dataRate);
    cmd.Parse(argc, argv);

    // Check the number of nodes
    if (nNodes <= 1) {
        std::cerr << "Number of nodes must be greater than 1 (one hub and at least one arm node)." << std::endl;
        return 1;
    }

    // Enable logging
    LogComponentEnable("TcpStarServer", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(nNodes);

    // Create point-to-point channels
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
    
    // Install queue monitoring on the hub node.
    // This must happen before the links are installed, so that the queue
    // gets traced on the channel.
    NodeContainer hubNode;
    hubNode.Add (nodes.Get (0)); // node 0 is the hub
    
    
    // Create net devices and channels
    NetDeviceContainer devices;
    for (int i = 1; i < nNodes; ++i) {
        NodeContainer pair;
        pair.Add(nodes.Get(0)); // Hub
        pair.Add(nodes.Get(i)); // Arm node

        NetDeviceContainer link = pointToPoint.Install(pair);
        devices.Add(link);
    }

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Create TCP sink application on the hub
    uint16_t port = 50000;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(0)); // Hub node
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Create CBR traffic source on the arm nodes
    OnOffHelper onoff("ns3::TcpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(0), port))); // Hub address
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoff.SetAttribute("DataRate", StringValue(dataRate));

    ApplicationContainer clientApps;
    for (int i = 1; i < nNodes; ++i) {
        clientApps.Add(onoff.Install(nodes.Get(i))); // Arm nodes
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable packet capture
    pointToPoint.EnablePcapAll("tcp-star-server");
    for (int i = 0; i < devices.GetN (); ++i)
    {
        Ptr<NetDevice> nd = devices.Get (i);
        std::string name = nd->GetInstanceTypeId ().GetName ();

        Ptr<PointToPointNetDevice> p2p = nd->GetObject<PointToPointNetDevice> ();
        if (p2p != 0)
        {
            // Get the interface index
            int interfaceIndex = i;

            // Extract node and interface number from interfaceIndex (not really needed here, but good practice)
            int nodeNumber = (interfaceIndex + 1);

            std::ostringstream oss;
            oss << "tcp-star-server-" << nodeNumber << "-" << 0 << ".pcap";  // Assuming interface number is always 0 for each arm
            pointToPoint.EnablePcap(oss.str(), nd, false); // Enable promiscuous mode for all packets.
         }
    }
    
    // Tracing
    AsciiTraceHelper ascii;
    pointToPoint.EnableAsciiAll(ascii.CreateFileStream("tcp-star-server.tr"));

    // Run the simulation
    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}