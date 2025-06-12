#include "ns3/test.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/socket.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SocketBoundRoutingExampleTest");

class SocketBoundRoutingExampleTest : public ns3::TestCase
{
public:
  SocketBoundRoutingExampleTest()
      : TestCase("Socket Bound Routing Example Test") {}

  void DoRun() override
  {
    // Run tests here
    TestCreateNodes();
    TestStaticRouting();
    TestSocketRecv();
    TestSendPacket();
    TestBindSocket();
  }

private:
  // Test the creation of nodes
  void TestCreateNodes()
  {
    // Create nodes
    Ptr<Node> nSrc = CreateObject<Node>();
    Ptr<Node> nDst = CreateObject<Node>();
    Ptr<Node> nRtr1 = CreateObject<Node>();
    Ptr<Node> nRtr2 = CreateObject<Node>();
    Ptr<Node> nDstRtr = CreateObject<Node>();

    NodeContainer c = NodeContainer(nSrc, nDst, nRtr1, nRtr2, nDstRtr);

    // Validate nodes are created
    NS_TEST_ASSERT_MSG_EQ(nSrc != nullptr, true, "Source node creation failed");
    NS_TEST_ASSERT_MSG_EQ(nDst != nullptr, true, "Destination node creation failed");
    NS_TEST_ASSERT_MSG_EQ(nRtr1 != nullptr, true, "Router 1 node creation failed");
    NS_TEST_ASSERT_MSG_EQ(nRtr2 != nullptr, true, "Router 2 node creation failed");
    NS_TEST_ASSERT_MSG_EQ(nDstRtr != nullptr, true, "Destination Router node creation failed");
  }

  // Test the static routing setup
  void TestStaticRouting()
  {
    // Create nodes and assign IP addresses
    Ptr<Node> nSrc = CreateObject<Node>();
    Ptr<Node> nRtr1 = CreateObject<Node>();
    Ptr<Node> nRtr2 = CreateObject<Node>();
    Ptr<Node> nDstRtr = CreateObject<Node>();
    Ptr<Node> nDst = CreateObject<Node>();

    InternetStackHelper internet;
    NodeContainer c = NodeContainer(nSrc, nRtr1, nRtr2, nDstRtr, nDst);
    internet.Install(c);

    // Set up point-to-point links and IP addresses (from original code)
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer dSrcdRtr1 = p2p.Install(NodeContainer(nSrc, nRtr1));
    NetDeviceContainer dSrcdRtr2 = p2p.Install(NodeContainer(nSrc, nRtr2));
    NetDeviceContainer dRtr1dDstRtr = p2p.Install(NodeContainer(nRtr1, nDstRtr));
    NetDeviceContainer dRtr2dDstRtr = p2p.Install(NodeContainer(nRtr2, nDstRtr));
    NetDeviceContainer dDstRtrdDst = p2p.Install(NodeContainer(nDstRtr, nDst));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ipv4.Assign(dSrcdRtr1);
    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    ipv4.Assign(dSrcdRtr2);
    ipv4.SetBase("10.10.1.0", "255.255.255.0");
    ipv4.Assign(dRtr1dDstRtr);
    ipv4.SetBase("10.10.2.0", "255.255.255.0");
    ipv4.Assign(dRtr2dDstRtr);
    ipv4.SetBase("10.20.1.0", "255.255.255.0");
    ipv4.Assign(dDstRtrdDst);

    Ptr<Ipv4> ipv4Src = nSrc->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> staticRoutingSrc = Ipv4StaticRoutingHelper().GetStaticRouting(ipv4Src);

    // Add static routes and check them
    staticRoutingSrc->AddHostRouteTo(Ipv4Address("10.20.1.2"), Ipv4Address("10.1.1.2"), 1);
    staticRoutingSrc->AddHostRouteTo(Ipv4Address("10.20.1.2"), Ipv4Address("10.1.2.2"), 2);

    NS_TEST_ASSERT_MSG_EQ(staticRoutingSrc->GetNRoutes(), 2, "Static routes setup failed");
  }

  // Test socket receiving functionality
  void TestSocketRecv()
  {
    Ptr<Socket> dstSocket = Socket::CreateSocket(CreateObject<Node>(), TypeId::LookupByName("ns3::UdpSocketFactory"));
    dstSocket->Bind(InetSocketAddress(Ipv4Address("10.20.1.2"), 12345));
    dstSocket->SetRecvCallback(MakeCallback(&dstSocketRecv));

    // Simulate packet reception
    Simulator::Schedule(Seconds(1.0), &SendStuff, dstSocket, Ipv4Address("10.20.1.2"), 12345);

    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_ASSERT_MSG_EQ(dstSocket->GetRecvCallback(), MakeCallback(&dstSocketRecv), "Socket receive callback mismatch");
  }

  // Test packet sending functionality
  void TestSendPacket()
  {
    Ptr<Socket> srcSocket = Socket::CreateSocket(CreateObject<Node>(), TypeId::LookupByName("ns3::UdpSocketFactory"));
    Ipv4Address dstAddr("10.20.1.2");
    uint16_t dstPort = 12345;
    SendStuff(srcSocket, dstAddr, dstPort);

    NS_TEST_ASSERT_MSG_EQ(srcSocket->SendTo(Create<Packet>(), 0, InetSocketAddress(dstAddr, dstPort)), true, "Packet send failed");
  }

  // Test binding socket to a network device
  void TestBindSocket()
  {
    Ptr<NetDevice> dev = CreateObject<NetDevice>();
    Ptr<Socket> sock = Socket::CreateSocket(CreateObject<Node>(), TypeId::LookupByName("ns3::UdpSocketFactory"));

    BindSock(sock, dev);
    NS_TEST_ASSERT_MSG_EQ(sock->GetBoundNetDevice() != nullptr, true, "Socket binding failed");
  }
};

int main(int argc, char *argv[])
{
  SocketBoundRoutingExampleTest test;
  test.Run();
  return 0;
}
