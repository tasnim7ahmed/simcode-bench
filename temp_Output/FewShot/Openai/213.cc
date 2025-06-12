#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

static uint64_t g_bytesSent = 0;
static uint64_t g_bytesReceived = 0;

void
TxCallback(Ptr<const Packet> packet)
{
    g_bytesSent += packet->GetSize();
}

void
RxCallback(Ptr<const Packet> packet, const Address &addr)
{
    g_bytesReceived += packet->GetSize();
}

int main(int argc, char *argv[])
{
    // Enable logging for BulkSend and PacketSink
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install the Internet Stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up packet tracing
    pointToPoint.EnablePcapAll("tcp_sim");

    // Set up TCP server (PacketSink) on node 1
    uint16_t port = 50000;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Set up TCP client (BulkSend) on node 0
    BulkSendHelper bulkSender("ns3::TcpSocketFactory", sinkAddress);
    bulkSender.SetAttribute("MaxBytes", UintegerValue(10000000));
    ApplicationContainer clientApp = bulkSender.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Connect trace sources to monitor sent and received bytes
    Ptr<Application> app = clientApp.Get(0);
    Ptr<BulkSendApplication> bulkSendApp = DynamicCast<BulkSendApplication>(app);
    if (bulkSendApp)
    {
        bulkSendApp->TraceConnectWithoutContext("Tx", MakeCallback(&TxCallback));
    }

    Ptr<Application> sinkApplication = sinkApp.Get(0);
    sinkApplication->TraceConnectWithoutContext("Rx", MakeCallback(&RxCallback));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    std::cout << "Bytes Sent: " << g_bytesSent << std::endl;
    std::cout << "Bytes Received: " << g_bytesReceived << std::endl;

    Simulator::Destroy();
    return 0;
}