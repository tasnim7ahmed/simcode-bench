#include <iostream>
#include <map>
#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

// Mock function for testing queue disc statistics.
void TestQueueDiscStats(Ptr<QueueDisc> q)
{
    NS_TEST_ASSERT_MSG_EQ(q->GetStats().GetNDroppedPackets(RedQueueDisc::UNFORCED_DROP), 0, "There should be no unforced drops.");
    NS_TEST_ASSERT_MSG_EQ(q->GetStats().GetNDroppedPackets(QueueDisc::INTERNAL_QUEUE_DROP), 0, "There should be no drops due to queue full.");
    std::cout << "Queue Disc Stats: " << q->GetStats() << std::endl;
}

// Mock function for testing flow statistics collection.
void TestFlowStats(Ptr<FlowMonitor> monitor)
{
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    
    NS_TEST_ASSERT_MSG_EQ(stats.size(), 1, "There should be exactly one flow.");
    double offeredLoad = stats[1].txBytes * 8.0 / (stats[1].timeLastTxPacket.GetSeconds() - stats[1].timeFirstTxPacket.GetSeconds()) / 1000000;
    NS_TEST_ASSERT_MSG_LT(offeredLoad, 10.0, "Offered load should be under 10 Mbps.");
    
    std::cout << "*** Flow Stats ***" << std::endl;
    std::cout << "  Tx Packets/Bytes:   " << stats[1].txPackets << " / " << stats[1].txBytes << std::endl;
    std::cout << "  Rx Packets/Bytes:   " << stats[1].rxPackets << " / " << stats[1].rxBytes << std::endl;
    std::cout << "  Throughput: " << stats[1].rxBytes * 8.0 / (stats[1].timeLastRxPacket.GetSeconds() - stats[1].timeFirstRxPacket.GetSeconds()) / 1000000 << " Mbps" << std::endl;
}

// Mock function for testing the throughput calculation.
void TestThroughput(double simulationTime, Ptr<Application> app)
{
    uint64_t totalPackets = DynamicCast<PacketSink>(app)->GetTotalRx();
    double throughput = totalPackets * 8.0 / (simulationTime * 1000000); // Mbit/s
    NS_TEST_ASSERT_MSG_LT(throughput, 50.0, "Throughput should be less than 50 Mbps.");
    std::cout << "*** Throughput Test ***" << std::endl;
    std::cout << "  Rx Bytes: " << totalPackets << std::endl;
    std::cout << "  Average Goodput: " << throughput << " Mbit/s" << std::endl;
}

// Test function to check if the DSCP values are correctly captured.
void TestDscpValues(Ptr<Ipv4FlowClassifier> classifier)
{
    std::vector<std::pair<uint32_t, uint32_t>> dscpVec = classifier->GetDscpCounts(1);
    NS_TEST_ASSERT_MSG_GT(dscpVec.size(), 0, "No DSCP values captured.");
    std::cout << "*** DSCP Values Test ***" << std::endl;
    for (auto &p : dscpVec)
    {
        std::cout << "  DSCP value:   0x" << std::hex << static_cast<uint32_t>(p.first) << std::dec << "  count:   " << p.second << std::endl;
    }
}

// Test function to check the flow monitoring of Rx bytes.
void TestFlowRxBytes(Ptr<FlowMonitor> monitor)
{
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    NS_TEST_ASSERT_MSG_GT(stats[1].rxBytes, 0, "No bytes received on flow.");
    std::cout << "*** Flow Rx Bytes Test ***" << std::endl;
    std::cout << "  Rx Bytes: " << stats[1].rxBytes << std::endl;
}

class TrafficControlTestSuite : public ns3::TestCase
{
public:
    TrafficControlTestSuite() : TestCase("Traffic Control Test Suite") {}

    virtual void DoRun() 
    {
        // Create nodes and configure the network
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
        pointToPoint.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("1p"));

        NetDeviceContainer devices;
        devices = pointToPoint.Install(nodes);

        InternetStackHelper stack;
        stack.Install(nodes);

        TrafficControlHelper tch;
        tch.SetRootQueueDisc("ns3::RedQueueDisc");
        QueueDiscContainer qdiscs = tch.Install(devices);

        Ptr<QueueDisc> q = qdiscs.Get(1);

        // Start the simulation
        Simulator::Run();

        // Run the tests
        TestQueueDiscStats(q);
        TestFlowStats(flowmon);
        TestThroughput(simulationTime, sinkApp.Get(0));
        TestDscpValues(classifier);
        TestFlowRxBytes(flowmon);

        // Destroy the simulation
        Simulator::Destroy();
    }
};

int main()
{
    TrafficControlTestSuite ts;
    ts.Run();
    return 0;
}
