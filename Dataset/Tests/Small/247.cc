#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

class NodeIPTestSuite : public TestCase {
public:
    NodeIPTestSuite() : TestCase("Test Node IP Assignment") {}
    virtual ~NodeIPTestSuite() {}

    void DoRun() override {
        TestNodeCreation();
        TestInternetStackInstallation();
        TestIPAssignment();
    }

private:
    // Test the creation of nodes in NodeContainer
    void TestNodeCreation() {
        NodeContainer nodes;
        nodes.Create(3);

        // Ensure the container has 3 nodes
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 3, "Node creation failed: Expected 3 nodes.");
    }

    // Test the installation of the Internet stack on all nodes
    void TestInternetStackInstallation() {
        NodeContainer nodes;
        nodes.Create(3);

        // Install Internet stack
        InternetStackHelper internet;
        internet.Install(nodes);

        // Verify that the Internet stack was installed on all nodes
        for (uint32_t i = 0; i < nodes.GetN(); ++i) {
            Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_NE(ipv4, nullptr, "Internet stack not installed on Node " << i);
        }
    }

    // Test the assignment of IP addresses to the nodes
    void TestIPAssignment() {
        NodeContainer nodes;
        nodes.Create(3);

        // Install Internet stack and assign IP addresses
        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(nodes);

        // Check that the assigned IP addresses are correct
        for (uint32_t i = 0; i < nodes.GetN(); ++i) {
            Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
            Ipv4Address ip = ipv4->GetAddress(1, 0);
            NS_TEST_ASSERT_MSG_EQ(ip, Ipv4Address("192.168.1." + std::to_string(i + 1)),
                                   "IP address mismatch for Node " << i << ": Expected 192.168.1." << (i + 1) << ", but got " << ip);
        }
    }
};

// Instantiate the test suite
static NodeIPTestSuite nodeIPTestSuite;
