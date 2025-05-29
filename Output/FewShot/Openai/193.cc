#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class DataReceiver : public Application
{
public:
    DataReceiver() : m_totalBytes(0) {}
    virtual ~DataReceiver() {}

    uint64_t GetTotalRx() const { return m_totalBytes; }

protected:
    virtual void StartApplication() override
    {
        if (!m_socket)
        {
            m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
            InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
            m_socket->Bind(local);
            m_socket->Listen();
            m_socket->SetAcceptCallback(
                MakeNullCallback<bool, Ptr<Socket>, const Address &>(),
                MakeCallback(&DataReceiver::HandleAccept, this));
        }
    }

    virtual void StopApplication() override
    {
        if (m_socket)
        {
            m_socket->Close();
        }
    }

    void HandleAccept(Ptr<Socket> s, const Address &from)
    {
        s->SetRecvCallback(MakeCallback(&DataReceiver::HandleRead, this));
    }

    void HandleRead(Ptr<Socket> socket)
    {
        while (Ptr<Packet> packet = socket->Recv())
        {
            m_totalBytes += packet->GetSize();
        }
    }

public:
    void SetPort(uint16_t port) { m_port = port; }

private:
    Ptr<Socket> m_socket;
    uint16_t m_port{8080};
    uint64_t m_totalBytes;
};

int main(int argc, char *argv[])
{
    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Setup point-to-point channel
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("5ms"));
    NetDeviceContainer devices = p2p.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Enable PCAP tracing (creates .pcap files)
    p2p.EnablePcapAll("tcp-p2p");

    // Install TCP receiver (custom DataReceiver) on node 1
    uint16_t port = 8080;
    Ptr<DataReceiver> receiverApp = CreateObject<DataReceiver>();
    receiverApp->SetPort(port);
    nodes.Get(1)->AddApplication(receiverApp);
    receiverApp->SetStartTime(Seconds(0.0));
    receiverApp->SetStopTime(Seconds(10.0));

    // Install TCP BulkSend application on node 0 (the sender)
    BulkSendHelper bulkSender("ns3::TcpSocketFactory",
                              InetSocketAddress(interfaces.GetAddress(1), port));
    bulkSender.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data

    ApplicationContainer senderApp = bulkSender.Install(nodes.Get(0));
    senderApp.Start(Seconds(1.0));
    senderApp.Stop(Seconds(10.0));

    // Set up callback to print total received at simulation end
    Simulator::Schedule(Seconds(10.01), [receiverApp]() {
        std::cout << "Total Bytes Received: " << receiverApp->GetTotalRx() << std::endl;
    });

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}