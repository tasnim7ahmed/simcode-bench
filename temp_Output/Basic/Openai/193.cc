#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class ReceiverApp : public Application
{
public:
  ReceiverApp () : m_totalRx (0) {}
  void Setup (Ptr<Socket> socket) { m_socket = socket; }
  uint64_t GetTotalRx () const { return m_totalRx; }

private:
  virtual void StartApplication () override
  {
    if (m_socket)
      {
        m_socket->SetRecvCallback (MakeCallback (&ReceiverApp::HandleRead, this));
      }
  }
  virtual void StopApplication () override
  {
    if (m_socket)
      {
        m_socket->Close ();
      }
  }
  void HandleRead (Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    while ((packet = socket->Recv ()))
      {
        m_totalRx += packet->GetSize ();
      }
  }

  Ptr<Socket> m_socket;
  uint64_t m_totalRx;
};

static void
LogTotalRx (Ptr<ReceiverApp> app)
{
  std::cout << "Total Bytes Received: " << app->GetTotalRx () << std::endl;
}

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices = p2p.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  uint16_t port = 5000;

  // Receiver: install a TCP socket and the custom ReceiverApp
  Ptr<Socket> recvSocket = Socket::CreateSocket (nodes.Get (1), TcpSocketFactory::GetTypeId ());
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), port);
  recvSocket->Bind (local);
  recvSocket->Listen ();

  Ptr<ReceiverApp> receiverApp = CreateObject<ReceiverApp> ();
  receiverApp->Setup (recvSocket);
  nodes.Get (1)->AddApplication (receiverApp);
  receiverApp->SetStartTime (Seconds (0.0));
  receiverApp->SetStopTime (Seconds (10.0));

  // Sender: BulkSend application
  BulkSendHelper bulk ("ns3::TcpSocketFactory",
                       InetSocketAddress (interfaces.GetAddress (1), port));
  bulk.SetAttribute ("MaxBytes", UintegerValue (0)); // unlimited

  ApplicationContainer apps = bulk.Install (nodes.Get (0));
  apps.Start (Seconds (0.0));
  apps.Stop (Seconds (10.0));

  // Enable pcap tracing
  p2p.EnablePcapAll ("ptp-tcp");

  Simulator::Stop (Seconds (10.0));
  Simulator::Schedule (Seconds (10.0), &LogTotalRx, receiverApp);

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}