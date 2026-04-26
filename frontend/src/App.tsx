import { useWebSocket } from './hooks/useWebSocket'
import { useRestPolling } from './hooks/useRestPolling'
import { useMockData } from './hooks/useMockData'
import { useMarsBackendSync } from './hooks/useMarsBackendSync'
import { useTitanBackendSync } from './hooks/useTitanBackendSync'
import { useTitanComm } from './hooks/useTitanComm'
import { Header } from './components/Header'
import { Layout } from './components/Layout'

export function App() {
  useWebSocket()
  useRestPolling()
  useMockData()
  useMarsBackendSync()
  useTitanBackendSync()
  useTitanComm()

  return (
    <div className="h-screen flex flex-col bg-space-950 text-gray-100 font-mono overflow-hidden">
      <Header />
      <Layout />
    </div>
  )
}
