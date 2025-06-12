#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FatTreeDataCenter");

int main (int argc, char *argv[])
{
    uint32_t k = 4; // Fat-tree parameter: # of ports per switch (must be even, e.g., 4)
    uint32_t pods = k;
    uint32_t edgePerPod = k/2;
    uint32_t aggPerPod = k/2;
    uint32_t coreSwitches = (k/2)*(k/2);
    uint32_t numEdge = pods * edgePerPod;
    uint32_t numAgg = pods * aggPerPod;
    uint32_t numCore = coreSwitches;
    uint32_t numServers = pods * edgePerPod * edgePerPod;

    CommandLine cmd;
    cmd.AddValue ("k", "Fat-tree parameter (must be even)", k);
    cmd.Parse (argc, argv);

    // Containers for all nodes
    NodeContainer coreSwitchNodes;
    coreSwitchNodes.Create (numCore);
    NodeContainer aggSwitchNodes;
    aggSwitchNodes.Create (numAgg);
    NodeContainer edgeSwitchNodes;
    edgeSwitchNodes.Create (numEdge);
    NodeContainer serverNodes;
    serverNodes.Create (numServers);

    // PointToPoint Links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("10Gbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

    // Internet stack
    InternetStackHelper internet;
    internet.Install (coreSwitchNodes);
    internet.Install (aggSwitchNodes);
    internet.Install (edgeSwitchNodes);
    internet.Install (serverNodes);

    // Track all net device containers for assigning IPs
    std::vector<NetDeviceContainer> linkNetDevices;

    // Core <-> Aggregation connections
    // Each core switch connects to one aggregation switch in each pod
    for (uint32_t i = 0; i < coreSwitches; ++i)
    {
        uint32_t group = i / (k/2); // identify core group
        for (uint32_t pod = 0; pod < pods; ++pod)
        {
            uint32_t agg = pod * aggPerPod + group;
            NetDeviceContainer ndc = p2p.Install (coreSwitchNodes.Get (i), aggSwitchNodes.Get (agg));
            linkNetDevices.push_back (ndc);
        }
    }

    // Aggregation <-> Edge connections (within same pod)
    for (uint32_t pod = 0; pod < pods; ++pod)
    {
        for (uint32_t agg = 0; agg < aggPerPod; ++agg)
        {
            for (uint32_t edge = 0; edge < edgePerPod; ++edge)
            {
                uint32_t aggIdx = pod * aggPerPod + agg;
                uint32_t edgeIdx = pod * edgePerPod + edge;
                NetDeviceContainer ndc = p2p.Install (aggSwitchNodes.Get (aggIdx), edgeSwitchNodes.Get (edgeIdx));
                linkNetDevices.push_back (ndc);
            }
        }
    }

    // Edge <-> Server connections
    for (uint32_t edge = 0; edge < numEdge; ++edge)
    {
        for (uint32_t h = 0; h < edgePerPod; ++h)
        {
            uint32_t serverIdx = edge * edgePerPod + h;
            NetDeviceContainer ndc = p2p.Install (edgeSwitchNodes.Get (edge), serverNodes.Get (serverIdx));
            linkNetDevices.push_back (ndc);
        }
    }

    // Assign IP Addresses
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> interfaces;
    uint32_t subnet = 0;
    for (auto &ndc : linkNetDevices)
    {
        std::ostringstream subnetStr;
        subnetStr << "10." << ((subnet>>8)&0xFF) << "." << (subnet&0xFF) << ".0";
        address.SetBase (Ipv4Address (subnetStr.str ().c_str ()), "255.255.255.0");
        Ipv4InterfaceContainer ifc = address.Assign (ndc);
        interfaces.push_back (ifc);
        subnet++;
    }

    // Install applications
    uint16_t port = 5000;
    double appStart = 1.0;
    double appStop = 12.0;
    ApplicationContainer sinkApps, srcApps;

    // Pair servers: first half senders, second half receivers
    for (uint32_t i = 0; i < numServers/2; ++i)
    {
        // Search for the link that attaches to server node i
        uint32_t linkIdx = coreSwitches*pods + pods*aggPerPod*edgePerPod + i;
        Ipv4Address localAddr = interfaces[linkIdx].GetAddress (1);
        PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", Address (InetSocketAddress (localAddr, port)));
        ApplicationContainer app = sinkHelper.Install (serverNodes.Get (i));
        app.Start (Seconds (appStart));
        app.Stop (Seconds (appStop));
        sinkApps.Add (app);

        // Corresponding sender at server i + numServers/2
        uint32_t senderIdx = i + numServers/2;
        linkIdx = coreSwitches*pods + pods*aggPerPod*edgePerPod + senderIdx;
        Ipv4Address remoteAddr = interfaces[linkIdx].GetAddress (1);

        OnOffHelper clientHelper ("ns3::TcpSocketFactory", Address (InetSocketAddress (localAddr, port)));
        clientHelper.SetAttribute ("DataRate", DataRateValue (DataRate ("1Gbps")));
        clientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
        clientHelper.SetAttribute ("StartTime", TimeValue (Seconds (appStart+0.1)));
        clientHelper.SetAttribute ("StopTime", TimeValue (Seconds (appStop-0.1)));
        clientHelper.SetAttribute ("MaxBytes", UintegerValue (0)); // unlimited

        ApplicationContainer clientApp = clientHelper.Install (serverNodes.Get (senderIdx));
        srcApps.Add (clientApp);
    }

    // Populate Routing Tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    // Set up NetAnim - assign positions to all nodes in layers
    AnimationInterface anim ("fattree.xml");

    double xDist=70, yDist=70;
    // Place Core Switches (top)
    for (uint32_t i = 0; i < numCore; ++i)
    {
        anim.SetConstantPosition (coreSwitchNodes.Get (i), 300 + (i% (k/2))*xDist, 60);
    }
    // Place Aggregation Switches
    for (uint32_t i = 0; i < numAgg; ++i)
    {
        uint32_t pod = i / aggPerPod;
        uint32_t pos = i % aggPerPod;
        anim.SetConstantPosition (aggSwitchNodes.Get (i), 200 + pod * xDist*1.4 + pos*15, 140);
    }
    // Place Edge Switches
    for (uint32_t i = 0; i < numEdge; ++i)
    {
        uint32_t pod = i / edgePerPod;
        uint32_t pos = i % edgePerPod;
        anim.SetConstantPosition (edgeSwitchNodes.Get (i), 200 + pod*xDist*1.4 + pos*15, 220);
    }
    // Place Servers
    for (uint32_t i = 0; i < numServers; ++i)
    {
        uint32_t edge = i / edgePerPod;
        uint32_t pos = i % edgePerPod;
        anim.SetConstantPosition (serverNodes.Get (i), 180 + edge*xDist*0.7 + pos*13, 300+pos*8);
    }

    Simulator::Stop (Seconds (appStop+1.0));
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}