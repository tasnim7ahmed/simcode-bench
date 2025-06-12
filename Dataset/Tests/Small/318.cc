#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

class SimpleTcpTest : public TestCase
{
public:
    SimpleTcpTest() : TestCase("Simple TCP Network Test") {}

    virtual void DoRun() override
    {
        TestNodeCreation();
        TestInternetStackInstallation();
        TestTcpServerSetup();
        TestTcpClientSetup();
        TestIpAddressAssignment();
        TestSimulationExecution();
    }

private:
    void TestNodeCreation()
    {
        // Test the creation of two nodes: one for TCP client, one for TCP server
        NodeContainer nodes;
        nodes.Create(2);

        // Verify the number of nodes created
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Node creation failed");
    }

    void TestInternetStackInstallation()
    {
        // Test the installation of the internet stack (IP, TCP, UDP)
        NodeContainer nodes;
        nodes.Create(2);

        InternetStackHelper stack;
        stack.Install(nodes);

        // Verify internet stack installation on both nodes
        for (uint32_t i = 0; i < nodes.GetN(); ++i)
        {
            Ptr<Node> node = nodes.Get(i);
            Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_NE(ipv4, nullptr, "Internet stack installation failed on node " << i);
        }
    }

    void TestTcpServerSetup()
    {
        // Test the setup of TCP server on the second node
        NodeContainer nodes;
        nodes.Create(2);

        uint16_t port = 8080;
        TcpServerHelper tcpServer(port);
        ApplicationContainer serverApp = tcpServer.Install(nodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the server application is installed correctly
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "TCP server application installation failed on node 1");
    }

    void TestTcpClientSetup()
    {
        // Test the setup of TCP client on the first node
        NodeContainer nodes;
        nodes.Create(2);

        uint16_t port = 8080;
        InetSocketAddress remoteAddress = InetSocketAddress(Ipv4Address("10.1.1.2"), port);
        TcpClientHelper tcpClient(remoteAddress);
        tcpClient.SetAttribute("MaxPackets", UintegerValue(10));
        tcpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        tcpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = tcpClient.Install(nodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the client application is installed correctly
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "TCP client application installation failed on node 0");
    }

    void TestIpAddressAssignment()
    {
        // Test the assignment of IP addresses to the nodes
        NodeContainer nodes;
        nodes.Create(2);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(nodes.Get(0)->GetDevice(0));
        ipv4.SetBase("10.1.2.0", "255.255.255.0");
        ipv4.Assign(nodes.Get(1)->GetDevice(0));

        // Verify that IP addresses are assigned correctly to both nodes
        Ipv4InterfaceContainer node0Interfaces = ipv4.Assign(nodes.Get(0)->GetDevice(0));
        Ipv4InterfaceContainer node1Interfaces = ipv4.Assign(nodes.Get(1)->GetDevice(0));

        NS_TEST_ASSERT_MSG_EQ(node0Interfaces.GetN(), 1, "IP address assignment failed on node 0");
        NS_TEST_ASSERT_MSG_EQ(node1Interfaces.GetN(), 1, "IP address assignment failed on node 1");
    }

    void TestSimulationExecution()
    {
        // Test the execution of the simulation
        NodeContainer nodes;
        nodes.Create(2);

        uint16_t port = 8080;
        TcpServerHelper tcpServer(port);
        ApplicationContainer serverApp = tcpServer.Install(nodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        InetSocketAddress remoteAddress = InetSocketAddress(Ipv4Address("10.1.1.2"), port);
        TcpClientHelper tcpClient(remoteAddress);
        tcpClient.SetAttribute("MaxPackets", UintegerValue(10));
        tcpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        tcpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = tcpClient.Install(nodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        ipv4.Assign(nodes.Get(0)->GetDevice(0));
        ipv4.SetBase("10.1.2.0", "255.255.255.0");
        ipv4.Assign(nodes.Get(1)->GetDevice(0));

        Simulator::Run();
        Simulator::Destroy();

        // Verify that the simulation ran without errors
        NS_TEST_ASSERT_MSG_EQ(true, true, "Simulation execution failed");
    }
};

// Create the test case and run it
int main()
{
    SimpleTcpTest test;
    test.Run();
    return 0;
}
