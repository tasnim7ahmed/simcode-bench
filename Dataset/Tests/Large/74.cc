#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/aodv-module.h"
#include "ns3/olsr-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/dsr-module.h"
#include "routing-experiment.h"

using namespace ns3;

class RoutingExperimentTest : public TestCase {
public:
    RoutingExperimentTest() : TestCase("Test RoutingExperiment Functionality") {}

    virtual void DoRun() {
        RoutingExperiment experiment;

        // Test Packet Reception
        Ptr<Node> node = CreateObject<Node>();
        Ptr<Socket> socket = Socket::CreateSocket(node, TypeId::LookupByName("ns3::UdpSocketFactory"));
        Ptr<Packet> packet = Create<Packet>(100);
        Address senderAddress;
        experiment.ReceivePacket(socket);
        NS_TEST_ASSERT_MSG_EQ(experiment.packetsReceived, 1, "Packet should be received");

        // Test Throughput Calculation
        experiment.bytesTotal = 8000; // 8KB of data received
        experiment.CheckThroughput();
        std::ifstream file("manet-routing.output.csv");
        NS_TEST_ASSERT_MSG_EQ(file.good(), true, "Throughput log file should be created");

        // Test Command Setup
        char* argv[] = { (char*)"program", (char*)"--protocol=AODV", (char*)"--CSVfileName=test.csv" };
        int argc = 3;
        experiment.CommandSetup(argc, argv);
        NS_TEST_ASSERT_MSG_EQ(experiment.m_protocolName, "AODV", "Protocol should be set to AODV");
        NS_TEST_ASSERT_MSG_EQ(experiment.m_CSVfileName, "test.csv", "CSV filename should be updated");
    }
};

static RoutingExperimentTest routingExperimentTestInstance;
